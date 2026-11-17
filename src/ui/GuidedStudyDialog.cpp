#include "GuidedStudyDialog.h"
#include "core/GuidedEngineeringStudies.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QStringList>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

GuidedStudyDialog::GuidedStudyDialog(EngineeringBodyContext body,QWidget* parent)
    : QDialog(parent),body_(std::move(body)) {
    setWindowTitle(QStringLiteral("Guided engineering studies | preliminary design"));
    resize(850,650);
    auto* main=new QVBoxLayout(this);
    auto* head=new QLabel(QStringLiteral("<b>Guided engineering studies</b> · multiple results, checks, and assumptions"),this);
    main->addWidget(head);
    auto* warning=new QLabel(QStringLiteral("Concept and preliminary sizing tools, not certified analysis. "
        "Inputs are engineering assumptions; bounding dimensions do not establish actual load paths or cross sections."),this);
    warning->setWordWrap(true);main->addWidget(warning);
    studies_=new QComboBox(this);
    const auto& catalog=cad::guidedEngineeringStudyCatalog();
    for(const auto& entry:catalog)
        studies_->addItem(QString::fromStdString(entry.group+" | "+entry.name));
    main->addWidget(studies_);
    assumptions_=new QLabel(this);
    assumptions_->setWordWrap(true);
    assumptions_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    main->addWidget(assumptions_);
    auto* scroll=new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* container=new QWidget(scroll);
    form_=new QFormLayout(container);
    scroll->setWidget(container);
    main->addWidget(scroll,2);
    auto* bodyButton=new QPushButton(QStringLiteral("Use applicable selected-body dimensions"),this);
    bodyButton->setEnabled(body_.valid);
    bodyButton->setToolTip(QStringLiteral("Copies width/height/depth from the selected body's axis-aligned bounding box."));
    main->addWidget(bodyButton);
    results_=new QPlainTextEdit(this);results_->setReadOnly(true);
    main->addWidget(results_,1);
    auto* footer=new QDialogButtonBox(QDialogButtonBox::Close,this);
    auto* copy=footer->addButton(QStringLiteral("Copy report"),QDialogButtonBox::ActionRole);
    auto* exportButton=footer->addButton(QStringLiteral("Export TXT..."),QDialogButtonBox::ActionRole);
    main->addWidget(footer);
    connect(studies_,&QComboBox::currentIndexChanged,this,[this](int){chooseStudy();});
    connect(bodyButton,&QPushButton::clicked,this,[this]{loadBodyDimensions();});
    connect(copy,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(report());});
    connect(exportButton,&QPushButton::clicked,this,[this]{
        const QString fileName=QFileDialog::getSaveFileName(this,QStringLiteral("Export engineering study"),
            QStringLiteral("engineering-study.txt"),QStringLiteral("Text (*.txt)"));
        if(fileName.isEmpty())return;
        QSaveFile file(fileName);
        const QByteArray bytes=report().toUtf8();
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text)||file.write(bytes)!=bytes.size()||!file.commit())
            QMessageBox::warning(this,QStringLiteral("Export failed"),QStringLiteral("Could not write the study report."));
    });
    connect(footer,&QDialogButtonBox::rejected,this,&QDialog::reject);
    chooseStudy();
}
void GuidedStudyDialog::chooseStudy(){
    const int index=studies_->currentIndex();
    const auto& catalog=cad::guidedEngineeringStudyCatalog();
    if(index<0||index>=static_cast<int>(catalog.size()))return;
    const auto& tool=catalog[static_cast<std::size_t>(index)];
    assumptions_->setText(QStringLiteral("Assumptions: ")+QString::fromStdString(tool.assumptions));
    fields_.clear();
    while(QLayoutItem* item=form_->takeAt(0)){
        if(auto* widget=item->widget())widget->deleteLater();
        delete item;
    }
    for(const auto& input:tool.inputs){
        auto* edit=new QDoubleSpinBox(this);
        edit->setDecimals(6);
        edit->setRange(input.minimum,input.maximum);
        edit->setValue(input.example);
        edit->setSingleStep(std::max(0.0001,std::abs(input.example)*0.05));
        edit->setSuffix(QStringLiteral("  ")+QString::fromStdString(input.unit));
        form_->addRow(QString::fromStdString(input.label),edit);
        fields_.push_back(edit);
        connect(edit,&QDoubleSpinBox::valueChanged,this,[this](double){refreshResults();});
    }
    refreshResults();
}
void GuidedStudyDialog::loadBodyDimensions(){
    if(!body_.valid||studies_->currentIndex()<0)return;
    const auto& tool=cad::guidedEngineeringStudyCatalog()[static_cast<std::size_t>(studies_->currentIndex())];
    for(std::size_t i=0;i<tool.inputs.size()&&i<fields_.size();++i){
        const auto& key=tool.inputs[i].key;
        double x=0;
        if(key=="width_mm")x=body_.xSizeMm;
        else if(key=="height_mm")x=body_.ySizeMm;
        else if(key=="length_mm")x=body_.zSizeMm;
        if(x>0&&x>=tool.inputs[i].minimum&&x<=tool.inputs[i].maximum)fields_[i]->setValue(x);
    }
    refreshResults();
}
QString GuidedStudyDialog::report() const {
    QStringList lines={QStringLiteral("CADWorkbench | Guided engineering study"),
        QStringLiteral("PRELIMINARY ESTIMATE — NOT A CODE-COMPLIANCE REPORT"),QString()};
    const int index=studies_->currentIndex();
    const auto& list=cad::guidedEngineeringStudyCatalog();
    if(index<0||index>=static_cast<int>(list.size()))return lines.join('\n');
    const auto& tool=list[static_cast<std::size_t>(index)];
    lines<<QStringLiteral("Study: ")+QString::fromStdString(tool.name);
    if(body_.valid)lines<<QStringLiteral("CAD reference: %1 (bounding dimensions only)").arg(body_.name);
    std::vector<double> values;
    for(std::size_t i=0;i<fields_.size();++i){
        const double x=fields_[i]->value();values.push_back(x);
        lines<<QStringLiteral("%1: %2 %3").arg(QString::fromStdString(tool.inputs[i].label))
               .arg(x,0,'g',12).arg(QString::fromStdString(tool.inputs[i].unit));
    }
    const auto result=cad::evaluateGuidedEngineeringStudy(tool.id,values);
    lines<<QString();
    if(result.valid){
        for(const auto& metric:result.metrics)
            lines<<QStringLiteral("%1: %2 %3").arg(QString::fromStdString(metric.name))
                .arg(metric.value,0,'g',12).arg(QString::fromStdString(metric.unit));
        lines<<QString();
        for(const auto& warning:result.warnings)lines<<QStringLiteral("LIMITATION: ")+QString::fromStdString(warning);
    }else lines<<QStringLiteral("INVALID INPUT: ")+QString::fromStdString(result.error);
    lines<<QStringLiteral("Assumptions: ")+QString::fromStdString(tool.assumptions);
    lines<<QStringLiteral("Verify load cases, safety factors, materials, fatigue and applicable codes before manufacture.");
    return lines.join('\n')+'\n';
}
void GuidedStudyDialog::refreshResults(){results_->setPlainText(report());}
