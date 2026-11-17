#include "EngineeringWorkbenchDialog.h"
#include "core/EngineeringDesignTools.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSplitter>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <cmath>
#include <vector>

EngineeringWorkbenchDialog::EngineeringWorkbenchDialog(EngineeringBodyContext body, QWidget* parent)
    : QDialog(parent), body_(std::move(body)) {
    setWindowTitle(QStringLiteral("Engineering Design Workbench · Preliminary Calculations"));
    resize(970,680);
    setMinimumSize(700,480);
    auto* main = new QVBoxLayout(this);
    auto* title = new QLabel(QStringLiteral("<b>Engineering Workbench</b> · 50 unit-labelled calculations across five groups"),this);
    main->addWidget(title);
    auto* disclaimer = new QLabel(QStringLiteral(
        "Preliminary estimates only. Check loading assumptions, local design codes, fatigue, buckling, "
        "material data, tolerances and safety factors independently before production."),this);
    disclaimer->setWordWrap(true);
    main->addWidget(disclaimer);

    auto* splitter = new QSplitter(Qt::Horizontal,this);
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    search_ = new QLineEdit(left);
    search_->setPlaceholderText(QStringLiteral("Search calculations, groups, or formulas…"));
    leftLayout->addWidget(search_);
    tools_ = new QListWidget(left);
    tools_->setAlternatingRowColors(true);
    leftLayout->addWidget(tools_);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    description_ = new QLabel(right);
    description_->setWordWrap(true);
    description_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rightLayout->addWidget(description_);
    bodyLabel_ = new QLabel(right);
    bodyLabel_->setWordWrap(true);
    bodyLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (body_.valid) {
        bodyLabel_->setText(QStringLiteral("Selected body: %1 · bounding X/Y/Z = %2 / %3 / %4 mm · volume = %5 mm³")
            .arg(body_.name)
            .arg(body_.xSizeMm,0,'g',7).arg(body_.ySizeMm,0,'g',7)
            .arg(body_.zSizeMm,0,'g',7).arg(body_.volumeMm3,0,'g',7));
        if(body_.assignedDensityKgPerM3>0.0&&std::isfinite(body_.assignedDensityKgPerM3))
            bodyLabel_->setText(bodyLabel_->text()+QStringLiteral(" · assigned density = %1 kg/m³ · estimated mass = %2 kg")
                .arg(body_.assignedDensityKgPerM3,0,'g',7)
                .arg(body_.volumeMm3*body_.assignedDensityKgPerM3*1e-9,0,'g',7));
    } else {
        bodyLabel_->setText(QStringLiteral("No selected solid. Enter dimensions directly."));
    }
    rightLayout->addWidget(bodyLabel_);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    auto* formContainer = new QWidget(scroll);
    form_ = new QFormLayout(formContainer);
    form_->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    scroll->setWidget(formContainer);
    rightLayout->addWidget(scroll,1);
    auto* loadBody = new QPushButton(QStringLiteral("Use selected body bounding dimensions"),right);
    loadBody->setEnabled(body_.valid);
    loadBody->setToolTip(QStringLiteral("Fills only X/Y/Z length, width, height and assigned density. "
                                     "A bounding box is not an equivalent structural cross section."));
    rightLayout->addWidget(loadBody);
    result_ = new QLabel(QStringLiteral("Choose a calculation"),right);
    result_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result_->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600; padding: 10px;"));
    rightLayout->addWidget(result_);
    notes_ = new QPlainTextEdit(right);
    notes_->setReadOnly(true);
    notes_->setMaximumHeight(130);
    notes_->setPlaceholderText(QStringLiteral("Calculation assumptions and units"));
    rightLayout->addWidget(notes_);
    auto* controls = new QHBoxLayout;
    auto* copy = new QPushButton(QStringLiteral("Copy report"),right);
    auto* save = new QPushButton(QStringLiteral("Export report…"),right);
    controls->addWidget(copy);
    controls->addWidget(save);
    controls->addStretch();
    rightLayout->addLayout(controls);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0,1);
    splitter->setStretchFactor(1,2);
    main->addWidget(splitter,1);
    auto* bottom = new QDialogButtonBox(QDialogButtonBox::Close,this);
    main->addWidget(bottom);

    connect(bottom,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(search_,&QLineEdit::textChanged,this,[this]{filterTools();});
    connect(tools_,&QListWidget::currentItemChanged,this,[this]{showSelectedTool();});
    connect(loadBody,&QPushButton::clicked,this,[this]{fillFromSelectedBody();});
    connect(copy,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(reportText());});
    connect(save,&QPushButton::clicked,this,[this]{
        const QString path = QFileDialog::getSaveFileName(this,QStringLiteral("Export preliminary engineering report"),
                                                         QStringLiteral("engineering-calculation.txt"),
                                                         QStringLiteral("Text files (*.txt)"));
        if(path.isEmpty())return;
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
            QMessageBox::warning(this,QStringLiteral("Export failed"),file.errorString());return;
        }
        const QByteArray bytes=reportText().toUtf8();
        if(file.write(bytes)!=bytes.size()||!file.commit())
            QMessageBox::warning(this,QStringLiteral("Export failed"),file.errorString());
    });
    filterTools();
}

void EngineeringWorkbenchDialog::filterTools() {
    const QString query=search_->text().trimmed();
    tools_->clear();
    const auto& catalog=cad::engineeringToolCatalog();
    for(int i=0;i<static_cast<int>(catalog.size());++i){
        const auto& entry=catalog[static_cast<std::size_t>(i)];
        const QString title=QString::fromStdString(entry.title);
        const QString group=QString::fromStdString(entry.group);
        const QString assumptions=QString::fromStdString(entry.assumptions);
        if(!query.isEmpty()&&!title.contains(query,Qt::CaseInsensitive)&&
           !group.contains(query,Qt::CaseInsensitive)&&!assumptions.contains(query,Qt::CaseInsensitive))continue;
        auto* row=new QListWidgetItem(group+QStringLiteral(" · ")+title,tools_);
        row->setData(Qt::UserRole,i);
        row->setToolTip(assumptions);
    }
    if(tools_->count()>0)tools_->setCurrentRow(0);
    else{
        currentToolIndex_=-1;
        result_->setText(QStringLiteral("No matching calculation"));
        description_->setText(QString());
        notes_->clear();
    }
}

void EngineeringWorkbenchDialog::showSelectedTool() {
    if(!tools_->currentItem())return;
    const int index=tools_->currentItem()->data(Qt::UserRole).toInt();
    const auto& catalog=cad::engineeringToolCatalog();
    if(index<0||index>=static_cast<int>(catalog.size()))return;
    currentToolIndex_=index;
    const auto& tool=catalog[static_cast<std::size_t>(index)];
    description_->setText(QStringLiteral("<b>%1</b><br>%2")
                          .arg(QString::fromStdString(tool.title).toHtmlEscaped(),
                               QString::fromStdString(tool.assumptions).toHtmlEscaped()));
    values_.clear();
    while(QLayoutItem* item=form_->takeAt(0)){
        if(QWidget* widget=item->widget())widget->deleteLater();
        delete item;
    }
    for(const auto& spec:tool.inputs){
        auto* edit=new QDoubleSpinBox(this);
        edit->setDecimals(6);
        edit->setRange(spec.minimum,spec.maximum);
        edit->setValue(spec.example);
        edit->setSingleStep(std::max(0.001,std::abs(spec.example)*0.05));
        edit->setSuffix(QStringLiteral("  ")+QString::fromStdString(spec.unit));
        edit->setToolTip(QStringLiteral("Input unit: ")+QString::fromStdString(spec.unit));
        form_->addRow(QString::fromStdString(spec.label),edit);
        values_.push_back(edit);
        connect(edit,&QDoubleSpinBox::valueChanged,this,[this](double){updateCalculation();});
    }
    updateCalculation();
}

void EngineeringWorkbenchDialog::updateCalculation(){
    const auto& catalog=cad::engineeringToolCatalog();
    if(currentToolIndex_<0||currentToolIndex_>=static_cast<int>(catalog.size()))return;
    const auto& tool=catalog[static_cast<std::size_t>(currentToolIndex_)];
    std::vector<double> inputs;
    for(const QDoubleSpinBox* field:values_)inputs.push_back(field->value());
    const auto result=cad::evaluateEngineeringTool(tool.id,inputs);
    result_->setText(result.valid?
        QStringLiteral("Result: %1 %2").arg(result.value,0,'g',10).arg(QString::fromStdString(tool.resultUnit)):
        QStringLiteral("Invalid: %1").arg(QString::fromStdString(result.error)));
    notes_->setPlainText(QStringLiteral("Assumptions: %1\nInputs must be entered in the labelled units. "
                                       "Do not use this as a certified structural or manufacturing design.")
                            .arg(QString::fromStdString(tool.assumptions)));
}

void EngineeringWorkbenchDialog::fillFromSelectedBody(){
    if(!body_.valid||currentToolIndex_<0)return;
    const auto& spec=cad::engineeringToolCatalog()[static_cast<std::size_t>(currentToolIndex_)];
    for(std::size_t i=0;i<spec.inputs.size()&&i<values_.size();++i){
        const std::string& key=spec.inputs[i].key;
        double value=0.0;
        if(key=="width_mm")value=body_.xSizeMm;
        else if(key=="height_mm")value=body_.ySizeMm;
        else if(key=="length_mm")value=body_.zSizeMm;
        else if(key=="density_kgm3")value=body_.assignedDensityKgPerM3;
        if(value>0.0)values_[i]->setValue(value);
    }
    updateCalculation();
}

QString EngineeringWorkbenchDialog::reportText() const {
    QString report=QStringLiteral("CADWorkbench Engineering Workbench\nPRELIMINARY ESTIMATE — NOT ENGINEERING CERTIFICATION\n\n");
    const auto& catalog=cad::engineeringToolCatalog();
    if(currentToolIndex_<0||currentToolIndex_>=static_cast<int>(catalog.size()))return report+QStringLiteral("No calculation selected.\n");
    const auto& tool=catalog[static_cast<std::size_t>(currentToolIndex_)];
    report+=QStringLiteral("Tool: %1\nGroup: %2\n\n")
        .arg(QString::fromStdString(tool.title),QString::fromStdString(tool.group));
    if(body_.valid)report+=QStringLiteral("Reference CAD body: %1 (bounding dimensions only; does not define a cross section)\n")
                                    .arg(body_.name);
    std::vector<double> inputs;
    for(std::size_t i=0;i<values_.size();++i){
        const double value=values_[i]->value();
        inputs.push_back(value);
        report+=QStringLiteral("%1: %2 %3\n").arg(QString::fromStdString(tool.inputs[i].label))
            .arg(value,0,'g',12).arg(QString::fromStdString(tool.inputs[i].unit));
    }
    const auto answer=cad::evaluateEngineeringTool(tool.id,inputs);
    if(answer.valid)report+=QStringLiteral("\nResult: %1 %2\n").arg(answer.value,0,'g',12)
        .arg(QString::fromStdString(tool.resultUnit));
    else report+=QStringLiteral("\nERROR: %1\n").arg(QString::fromStdString(answer.error));
    report+=QStringLiteral("\nAssumptions: %1\nVerify model geometry, load cases, units, materials, applicable standards, and safety factors.\n")
        .arg(QString::fromStdString(tool.assumptions));
    return report;
}
