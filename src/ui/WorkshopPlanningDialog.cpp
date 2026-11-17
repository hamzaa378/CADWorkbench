#include "WorkshopPlanningDialog.h"
#include "core/WorkshopEngineering.h"
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QClipboard>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

namespace {
QString num(double v){return QString::number(v,'g',12);}
QString csvCell(QString s){
    s.replace(QLatin1Char('"'),QStringLiteral("\"\""));
    if(s.contains(QLatin1Char(','))||s.contains(QLatin1Char('"'))||s.contains(QLatin1Char('\n')))
        return QStringLiteral("\"")+s+QStringLiteral("\"");
    return s;
}
QDoubleSpinBox* spin(QWidget* owner,double initial,double min=0,double max=100000000,int decimals=5){
    auto* x=new QDoubleSpinBox(owner);
    x->setDecimals(decimals);x->setRange(min,max);x->setValue(initial);x->setSingleStep(std::max(0.01,std::abs(initial)/10));
    x->setKeyboardTracking(false);return x;
}
QPlainTextEdit* report(QWidget* parent){
    auto* v=new QPlainTextEdit(parent);v->setReadOnly(true);v->setMinimumHeight(125);return v;
}
void addToleranceRow(QTableWidget* table,QString label,double nominal,double lower,double upper,int direction){
    const int row=table->rowCount();table->insertRow(row);
    const QStringList values={label,num(nominal),num(lower),num(upper),direction==1?QStringLiteral("+"):QStringLiteral("-")};
    for(int col=0;col<values.size();++col)table->setItem(row,col,new QTableWidgetItem(values[col]));
}
}
WorkshopPlanningDialog::WorkshopPlanningDialog(const cad::Part* body,QWidget* parent):QDialog(parent){
    setWindowTitle(QStringLiteral("Engineering workshop | sections · machining · tolerances · model inventory"));
    resize(1050,740);
    auto* root=new QVBoxLayout(this);
    auto* notice=new QLabel(QStringLiteral("PRELIMINARY DESIGN WORKSHEETS. No machine-ready NC output, safety certification, or automatic hole recognition. Check feeds and fits against shop equipment and process data."),this);
    notice->setWordWrap(true);root->addWidget(notice);
    tabs_=new QTabWidget(this);root->addWidget(tabs_,1);

    // Parametric cross-section library.
    auto* section=new QWidget(tabs_);auto* sectionLayout=new QVBoxLayout(section);
    auto* sectionInputs=new QFormLayout;
    sectionProfile_=new QComboBox(section);
    for(const std::string& name:cad::workshop::sectionProfiles())
        sectionProfile_->addItem(QString::fromStdString(name).replace('_',' '),QString::fromStdString(name));
    sectionWidth_=spin(section,60);sectionHeight_=spin(section,40);
    sectionWall_=spin(section,4);sectionFlange_=spin(section,6);
    density_=spin(section,7850,0,100000);
    sectionInputs->addRow(QStringLiteral("Section family"),sectionProfile_);
    sectionInputs->addRow(QStringLiteral("Outside width / diameter (mm)"),sectionWidth_);
    sectionInputs->addRow(QStringLiteral("Outside height (mm)"),sectionHeight_);
    sectionInputs->addRow(QStringLiteral("Wall / web thickness (mm)"),sectionWall_);
    sectionInputs->addRow(QStringLiteral("Flange thickness (mm)"),sectionFlange_);
    sectionInputs->addRow(QStringLiteral("Material density (kg/m³; 0 = unknown)"),density_);
    sectionLayout->addLayout(sectionInputs);
    auto* runSectionButton=new QPushButton(QStringLiteral("Calculate centroid, inertias and section moduli"),section);
    sectionLayout->addWidget(runSectionButton);
    sectionReport_=report(section);sectionLayout->addWidget(sectionReport_,1);
    tabs_->addTab(section,QStringLiteral("Section properties"));
    connect(runSectionButton,&QPushButton::clicked,this,&WorkshopPlanningDialog::evaluateSection);

    // Subprocess-independent cutting estimate. No executable toolpaths.
    auto* machining=new QWidget(tabs_);auto* machiningLayout=new QVBoxLayout(machining);
    auto* machiningScroll=new QScrollArea(machining);
    machiningScroll->setWidgetResizable(true);
    auto* machiningInner=new QWidget(machiningScroll);auto* machiningForm=new QFormLayout(machiningInner);
    machiningOperation_=new QComboBox(machiningInner);
    for(const std::string& name:cad::workshop::machiningOperations())
        machiningOperation_->addItem(QString::fromStdString(name).replace('_',' '),QString::fromStdString(name));
    toolDiameter_=spin(machiningInner,10);travel_=spin(machiningInner,30);
    cutSpeed_=spin(machiningInner,30);feedRev_=spin(machiningInner,0.15,0,100,5);
    feedTooth_=spin(machiningInner,0.06,0,10,5);
    teeth_=new QSpinBox(machiningInner);teeth_->setRange(1,1000);teeth_->setValue(4);
    passes_=new QSpinBox(machiningInner);passes_->setRange(1,10000);passes_->setValue(1);
    engagementWidth_=spin(machiningInner,5);cutDepth_=spin(machiningInner,2);
    maxRpm_=spin(machiningInner,6000);energy_=spin(machiningInner,2.5,0,1e8);
    machinePower_=spin(machiningInner,5,0,1e8);
    machiningForm->addRow(QStringLiteral("Process"),machiningOperation_);
    machiningForm->addRow(QStringLiteral("Cutter diameter (mm)"),toolDiameter_);
    machiningForm->addRow(QStringLiteral("Nominal travel per pass (mm)"),travel_);
    machiningForm->addRow(QStringLiteral("Surface cutting speed (m/min)"),cutSpeed_);
    machiningForm->addRow(QStringLiteral("Feed per revolution / pitch (mm/rev)"),feedRev_);
    machiningForm->addRow(QStringLiteral("Feed per tooth, milling only (mm/tooth)"),feedTooth_);
    machiningForm->addRow(QStringLiteral("Milling teeth"),teeth_);
    machiningForm->addRow(QStringLiteral("Passes"),passes_);
    machiningForm->addRow(QStringLiteral("Engagement width, milling/turning (mm)"),engagementWidth_);
    machiningForm->addRow(QStringLiteral("Engagement depth, milling/turning (mm)"),cutDepth_);
    machiningForm->addRow(QStringLiteral("Spindle RPM limit (0 = unspecified)"),maxRpm_);
    machiningForm->addRow(QStringLiteral("Specific cutting energy (J/mm³; 0 = unknown)"),energy_);
    machiningForm->addRow(QStringLiteral("Available spindle power (kW; 0 = unknown)"),machinePower_);
    machiningScroll->setWidget(machiningInner);
    machiningLayout->addWidget(machiningScroll,2);
    auto* runMachiningButton=new QPushButton(QStringLiteral("Estimate RPM, feed, nominal cycle and power"),machining);
    machiningLayout->addWidget(runMachiningButton);
    machiningReport_=report(machining);machiningLayout->addWidget(machiningReport_,2);
    tabs_->addTab(machining,QStringLiteral("Machining estimates"));
    connect(runMachiningButton,&QPushButton::clicked,this,&WorkshopPlanningDialog::evaluateMachining);

    // Signed-dimensional chain, with worst-case and 3-sigma RSS shown separately.
    auto* tolerance=new QWidget(tabs_);auto* toleranceLayout=new QVBoxLayout(tolerance);
    toleranceLayout->addWidget(new QLabel(QStringLiteral("Editable rows: direction is + or -, and lower/upper are offsets from nominal in millimetres."),tolerance));
    toleranceTable_=new QTableWidget(0,5,tolerance);
    toleranceTable_->setHorizontalHeaderLabels({QStringLiteral("Dimension"),QStringLiteral("Nominal (mm)"),
        QStringLiteral("Lower deviation (mm)"),QStringLiteral("Upper deviation (mm)"),QStringLiteral("Direction +/-")});
    toleranceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    toleranceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    addToleranceRow(toleranceTable_,QStringLiteral("Housing"),25,-0.04,0.04,1);
    addToleranceRow(toleranceTable_,QStringLiteral("Spacer"),8,-0.02,0.02,-1);
    toleranceLayout->addWidget(toleranceTable_,2);
    auto* toleranceButtons=new QHBoxLayout;
    auto* addButton=new QPushButton(QStringLiteral("Add dimension"),tolerance);
    auto* removeButton=new QPushButton(QStringLiteral("Remove selected row"),tolerance);
    auto* calcButton=new QPushButton(QStringLiteral("Calculate worst case and RSS"),tolerance);
    toleranceButtons->addWidget(addButton);toleranceButtons->addWidget(removeButton);toleranceButtons->addWidget(calcButton);
    toleranceLayout->addLayout(toleranceButtons);
    toleranceReport_=report(tolerance);toleranceLayout->addWidget(toleranceReport_,1);
    tabs_->addTab(tolerance,QStringLiteral("Tolerance stack"));
    connect(addButton,&QPushButton::clicked,this,[this]{addToleranceRow(toleranceTable_,QStringLiteral("Dimension"),0,0,0,1);});
    connect(removeButton,&QPushButton::clicked,this,[this]{
        const int row=toleranceTable_->currentRow();if(row>=0)toleranceTable_->removeRow(row);
    });
    connect(calcButton,&QPushButton::clicked,this,&WorkshopPlanningDialog::evaluateTolerances);

    // Read-only CAD interrogation from actual B-rep geometry.
    auto* cylinders=new QWidget(tabs_);auto* cylinderLayout=new QVBoxLayout(cylinders);
    cylinderLayout->addWidget(new QLabel(QStringLiteral("Cylinder faces are NOT automatically classified as through-holes. Face numbers may change when topology changes."),cylinders));
    cylinderReport_=report(cylinders);cylinderReport_->setMinimumHeight(360);
    cylinderLayout->addWidget(cylinderReport_,1);
    auto* cylinderSelect=new QHBoxLayout;
    cylinderPick_=new QComboBox(cylinders);
    auto* useCylinder=new QPushButton(QStringLiteral("Use selected cylindrical face as a machining estimate (verify bore manually)"),cylinders);
    cylinderSelect->addWidget(cylinderPick_,1);cylinderSelect->addWidget(useCylinder);
    cylinderLayout->addLayout(cylinderSelect);
    connect(useCylinder,&QPushButton::clicked,this,[this]{
        const int index=cylinderPick_->currentData().toInt();
        if(index<0||index>=static_cast<int>(cylinders_.cylinders.size())){
            QMessageBox::information(this,QStringLiteral("No cylindrical surface"),QStringLiteral("Select a cylinder face from the inventory."));return;
        }
        const auto& c=cylinders_.cylinders[index];
        toolDiameter_->setValue(c.diameterMm);
        if(c.axialSpanMm>0)travel_->setValue(c.axialSpanMm);
        machiningOperation_->setCurrentIndex(machiningOperation_->findData(QStringLiteral("drill")));
        tabs_->setCurrentIndex(1);
        evaluateMachining();
        QMessageBox::information(this,QStringLiteral("Verify machining intent"),
            QStringLiteral("Copied face %1 diameter and parametric span as ESTIMATES. Cylinders may be bosses/shafts; this does not prove a hole, drill access, wall thickness or cutting depth.").arg(c.faceIndex));
    });
    tabs_->addTab(cylinders,QStringLiteral("CAD cylinder inventory"));

    auto* readiness=new QWidget(tabs_);auto* readinessLayout=new QVBoxLayout(readiness);
    readinessReport_=report(readiness);readinessLayout->addWidget(readinessReport_,1);
    tabs_->addTab(readiness,QStringLiteral("Model readiness"));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);
    auto* exportButton=buttons->addButton(QStringLiteral("Export current worksheet..."),QDialogButtonBox::ActionRole);
    auto* copyButton=buttons->addButton(QStringLiteral("Copy current worksheet"),QDialogButtonBox::ActionRole);
    root->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(exportButton,&QPushButton::clicked,this,&WorkshopPlanningDialog::exportCurrentReport);
    connect(copyButton,&QPushButton::clicked,this,[this]{
        const auto pages=std::vector<QPlainTextEdit*>{sectionReport_,machiningReport_,toleranceReport_,cylinderReport_,readinessReport_};
        const int tab=tabs_->currentIndex();
        if(tab>=0&&tab<static_cast<int>(pages.size()))QApplication::clipboard()->setText(tab==3?cylinderCsv():pages[tab]->toPlainText());
    });

    if(body&&!body->shape.IsNull()){
        bodyName_=body->name;
        Bnd_Box bounds;BRepBndLib::AddOptimal(body->shape,bounds,false,false);
        if(!bounds.IsVoid()){
            double x0,y0,z0,x1,y1,z1;
            bounds.Get(x0,y0,z0,x1,y1,z1);
            if(std::isfinite(x1-x0)&&std::isfinite(y1-y0)&&x1>x0&&y1>y0){
                sectionWidth_->setValue(x1-x0);sectionHeight_->setValue(y1-y0);
                travel_->setValue(std::max(1.0,z1-z0));
            }
        }
        if(body->materialDensityKgPerM3>0)density_->setValue(body->materialDensityKgPerM3);
        std::string issue;
        const bool measured=cad::workshop::inspectCylindricalSurfaces(body->shape,&cylinders_,&issue);
        QStringList lines;
        lines<<QStringLiteral("SELECTED BODY: %1").arg(bodyName_)
             <<QStringLiteral("Part number: %1").arg(body->partNumber)
             <<QStringLiteral("Revision: %1").arg(body->revision)
             <<QStringLiteral("Material: %1; density: %2 kg/m³").arg(body->materialName,num(body->materialDensityKgPerM3))
             <<QStringLiteral("Document metadata and source geometry are read-only in this workspace.");
        if(body->isSurfaceBody)lines<<QStringLiteral("WARNING: surface body; solid manufacturing assumptions may not apply.");
        if(body->materialDensityKgPerM3<=0||body->materialName.isEmpty())lines<<QStringLiteral("WARNING: Material/density are not fully assigned.");
        if(body->partNumber.isEmpty())lines<<QStringLiteral("WARNING: No part number. Set one before issuing drawings.");
        if(body->revision.isEmpty())lines<<QStringLiteral("WARNING: No part revision.");
        if(measured){
            GProp_GProps physical;
            if(!body->isSurfaceBody){
                BRepGProp::VolumeProperties(body->shape,physical);
                const double volume=physical.Mass();
                lines<<QStringLiteral("Modeled solid volume: %1 mm³").arg(num(volume));
                if(body->materialDensityKgPerM3>0&&std::isfinite(volume))
                    lines<<QStringLiteral("Nominal uniform-density mass: %1 kg").arg(num(volume*body->materialDensityKgPerM3*1e-9));
                if(!bounds.IsVoid()){
                    double x0,y0,z0,x1,y1,z1;bounds.Get(x0,y0,z0,x1,y1,z1);
                    const double block=(x1-x0)*(y1-y0)*(z1-z0);
                    if(block>0&&std::isfinite(block)&&volume>=0&&volume<=block*(1+1e-6)){
                        lines<<QStringLiteral("Bounding-block stock volume (approx.): %1 mm³").arg(num(block))
                             <<QStringLiteral("Bounding-block material use: %1 % (not a process simulation)").arg(num(100*volume/block));
                    }
                }
            }
            lines<<QStringLiteral("Topology: %1; unique faces: %2; solids: %3; cylindrical faces: %4")
                     .arg(cylinders_.validTopology?QStringLiteral("VALID"):QStringLiteral("INVALID"))
                     .arg(cylinders_.faces).arg(cylinders_.solids).arg(cylinders_.cylinders.size());
            if(!cylinders_.validTopology)lines<<QStringLiteral("CRITICAL: CAD B-rep checker reported invalid topology.");
            QStringList cylinderLines;
            cylinderLines<<QStringLiteral("Cylindrical surfaces in %1").arg(bodyName_)
                <<QStringLiteral("Face | Diameter mm | Axial parameter span mm | Axis midpoint mm | Axis direction | Area mm² | Reversed");
            int cylinderIndex=0;
            for(const auto& c:cylinders_.cylinders){
                cylinderPick_->addItem(QStringLiteral("Face %1 · ø%2 mm").arg(c.faceIndex).arg(num(c.diameterMm)),cylinderIndex++);
                cylinderLines<<QStringLiteral("%1 | %2 | %3 | (%4,%5,%6) | (%7,%8,%9) | %10 | %11")
                    .arg(c.faceIndex).arg(num(c.diameterMm)).arg(num(c.axialSpanMm))
                    .arg(num(c.centerXmm)).arg(num(c.centerYmm)).arg(num(c.centerZmm))
                    .arg(num(c.axisX)).arg(num(c.axisY)).arg(num(c.axisZ))
                    .arg(num(c.lateralAreaMm2)).arg(c.reversedOrientation?QStringLiteral("yes"):QStringLiteral("no"));
            }
            for(const auto& warning:cylinders_.warnings)cylinderLines<<QStringLiteral("NOTE: ")+QString::fromStdString(warning);
            cylinderReport_->setPlainText(cylinderLines.join('\n'));
        }else{
            cylinderReport_->setPlainText(QStringLiteral("Unable to inspect selected body: ")+QString::fromStdString(issue));
            lines<<QStringLiteral("CRITICAL: Cylindrical inspection failed: ")+QString::fromStdString(issue);
        }
        lines<<QStringLiteral("MANUFACTURING RELEASE IS NOT APPROVED BY THIS REPORT. Verify datum scheme, tolerances, hole depth and access, tool holding, fixtures, drawing issue and applicable standards.");
        readinessReport_->setPlainText(lines.join('\n'));
    }else{
        cylinderReport_->setPlainText(QStringLiteral("Select one solid body in the model browser, then reopen the workshop to inspect cylinders."));
        readinessReport_->setPlainText(QStringLiteral("No selected CAD body. Calculators remain available; no model dimensions or material can be inferred."));
    }
    evaluateSection();evaluateMachining();evaluateTolerances();
}

void WorkshopPlanningDialog::evaluateSection(){
    cad::workshop::SectionSpec s;
    s.profile=sectionProfile_->currentData().toString().toStdString();
    s.widthMm=sectionWidth_->value();s.heightMm=sectionHeight_->value();
    s.wallMm=sectionWall_->value();s.flangeMm=sectionFlange_->value();s.densityKgPerM3=density_->value();
    cad::workshop::SectionResult v;std::string issue;
    if(!cad::workshop::calculateSection(s,&v,&issue)){
        sectionReport_->setPlainText(QStringLiteral("INVALID SECTION: ")+QString::fromStdString(issue));return;
    }
    const QStringList lines={QStringLiteral("PRELIMINARY SECTION PROPERTIES | mm, mm², mm³, mm⁴"),
       QStringLiteral("Profile: %1").arg(QString::fromStdString(s.profile)),
       QStringLiteral("Area: %1 mm²").arg(num(v.areaMm2)),
       QStringLiteral("Centroid local X,Y: %1, %2 mm").arg(num(v.centroidXmm),num(v.centroidYmm)),
       QStringLiteral("Ixx: %1 mm⁴ | Iyy: %2 mm⁴ | Ixy: %3 mm⁴").arg(num(v.secondMomentXmm4),num(v.secondMomentYmm4),num(v.productMomentXYmm4)),
       QStringLiteral("Polar AREA second moment (Ixx + Iyy): %1 mm⁴ (NOT torsional stiffness J for open sections)").arg(num(v.polarAreaMomentMm4)),
       QStringLiteral("Elastic Wx: %1 mm³ | Wy: %2 mm³").arg(num(v.modulusXmm3),num(v.modulusYmm3)),
       QStringLiteral("Radii of gyration X,Y: %1, %2 mm").arg(num(v.gyrationXmm),num(v.gyrationYmm)),
       QStringLiteral("Mass per metre: %1 kg/m (zero if density unknown)").arg(num(v.massKgPerMeter)),
       QStringLiteral("Nominal sharp-corner idealization: section radii, welds, local buckling and standards are not considered.")};
    sectionReport_->setPlainText(lines.join('\n'));
}
void WorkshopPlanningDialog::evaluateMachining(){
    cad::workshop::MachiningSpec s;
    s.operation=machiningOperation_->currentData().toString().toStdString();
    s.toolDiameterMm=toolDiameter_->value();s.travelMm=travel_->value();
    s.cuttingSpeedMMin=cutSpeed_->value();s.feedPerRevMm=feedRev_->value();s.feedPerToothMm=feedTooth_->value();
    s.teeth=teeth_->value();s.passes=passes_->value();s.radialWidthMm=engagementWidth_->value();s.depthMm=cutDepth_->value();
    s.maxSpindleRpm=maxRpm_->value();s.materialDensityKgM3=density_->value();
    s.specificCuttingEnergyJMm3=energy_->value();s.availablePowerKw=machinePower_->value();
    cad::workshop::MachiningResult v;std::string issue;
    if(!cad::workshop::estimateMachining(s,&v,&issue)){
        machiningReport_->setPlainText(QStringLiteral("INVALID PROCESS ESTIMATE: ")+QString::fromStdString(issue));return;
    }
    QStringList lines={QStringLiteral("PRELIMINARY MACHINING ESTIMATE: %1").arg(QString::fromStdString(s.operation)),
        QStringLiteral("Spindle: %1 RPM; actual cutting speed: %2 m/min").arg(num(v.spindleRpm),num(v.actualCuttingSpeedMMin)),
        QStringLiteral("Programmed nominal feed: %1 mm/min").arg(num(v.feedMmMin)),
        QStringLiteral("Nominal cutting time (no overheads): %1 min").arg(num(v.cycleTimeMin)),
        QStringLiteral("Nominal cutter engagement volume: %1 mm³").arg(num(v.nominalEngagementVolumeMm3)),
        QStringLiteral("Nominal rate: %1 mm³/min; nominal chips: %2 kg").arg(num(v.estimatedRemovalRateMm3Min),num(v.nominalChipMassKg)),
        QStringLiteral("Approximate cutting power: %1 kW").arg(num(v.approximatePowerKw))};
    for(const auto& warning:v.warnings)lines<<QStringLiteral("WARNING: ")+QString::fromStdString(warning);
    machiningReport_->setPlainText(lines.join('\n'));
}
void WorkshopPlanningDialog::evaluateTolerances(){
    std::vector<cad::workshop::ToleranceTerm> terms;
    for(int row=0;row<toleranceTable_->rowCount();++row){
        const auto cell=[&](int col){const auto* item=toleranceTable_->item(row,col);return item?item->text().trimmed():QString();};
        bool okN=false,okL=false,okU=false;
        cad::workshop::ToleranceTerm t;
        t.label=cell(0).toStdString();t.nominalMm=cell(1).toDouble(&okN);
        t.lowerDeviationMm=cell(2).toDouble(&okL);t.upperDeviationMm=cell(3).toDouble(&okU);
        t.direction=cell(4)==QStringLiteral("+")?1:cell(4)==QStringLiteral("-")?-1:0;
        if(!okN||!okL||!okU){
            toleranceReport_->setPlainText(QStringLiteral("Invalid numeric entry in row %1; use plain numbers in millimetres.").arg(row+1));return;
        }
        terms.push_back(std::move(t));
    }
    cad::workshop::ToleranceResult v;std::string issue;
    if(!cad::workshop::calculateToleranceStack(terms,&v,&issue)){
        toleranceReport_->setPlainText(QStringLiteral("INVALID TOLERANCE CHAIN: ")+QString::fromStdString(issue));return;
    }
    QStringList lines={QStringLiteral("TOLERANCE CHAIN: %1 dimensions").arg(terms.size()),
        QStringLiteral("Nominal result: %1 mm").arg(num(v.nominalMm)),
        QStringLiteral("Worst-case minimum: %1 mm").arg(num(v.worstMinMm)),
        QStringLiteral("Worst-case maximum: %1 mm").arg(num(v.worstMaxMm)),
        QStringLiteral("Estimated RSS center: %1 mm").arg(num(v.rssCenterMm)),
        QStringLiteral("Estimated RSS ± band (conditional 3σ): %1 mm").arg(num(v.rssHalfWidthMm))};
    for(const auto& warning:v.warnings)lines<<QStringLiteral("ASSUMPTION: ")+QString::fromStdString(warning);
    toleranceReport_->setPlainText(lines.join('\n'));
}
QString WorkshopPlanningDialog::cylinderCsv() const {
    QStringList lines={QStringLiteral("body,face_index,diameter_mm,parametric_span_mm,axis_center_x_mm,axis_center_y_mm,axis_center_z_mm,axis_x,axis_y,axis_z,area_mm2,reversed_orientation")};
    for(const auto& c:cylinders_.cylinders){
        const QStringList cells={csvCell(bodyName_),QString::number(c.faceIndex),num(c.diameterMm),num(c.axialSpanMm),
            num(c.centerXmm),num(c.centerYmm),num(c.centerZmm),num(c.axisX),num(c.axisY),num(c.axisZ),
            num(c.lateralAreaMm2),c.reversedOrientation?QStringLiteral("true"):QStringLiteral("false")};
        lines<<cells.join(',');
    }
    return lines.join('\n')+'\n';
}
void WorkshopPlanningDialog::exportCurrentReport(){
    const int tab=tabs_->currentIndex();
    const auto reports=std::vector<QPlainTextEdit*>{sectionReport_,machiningReport_,toleranceReport_,cylinderReport_,readinessReport_};
    if(tab<0||tab>=static_cast<int>(reports.size()))return;
    const bool csv=tab==3;
    const QString data=csv?cylinderCsv():reports[tab]->toPlainText()+'\n';
    const QString defaultName=csv?QStringLiteral("cylindrical_surface_inventory.csv"):QStringLiteral("engineering_workshop_report.txt");
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Export workshop worksheet"),defaultName,
                                                    csv?QStringLiteral("CSV (*.csv)"):QStringLiteral("Text (*.txt)"));
    if(path.isEmpty())return;
    QSaveFile file(path);
    const QByteArray bytes=data.toUtf8();
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)||file.write(bytes)!=bytes.size()||!file.commit())
        QMessageBox::warning(this,QStringLiteral("Export failed"),QStringLiteral("Unable to save engineering worksheet."));
}
