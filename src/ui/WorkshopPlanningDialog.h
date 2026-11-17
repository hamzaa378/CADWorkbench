#pragma once
#include "core/SceneDocument.h"
#include "core/WorkshopGeometry.h"
#include <QDialog>
class QTabWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPlainTextEdit;
class QTableWidget;

// Engineering planning worksheets. Never modifies the source document or emits NC code.
class WorkshopPlanningDialog final : public QDialog {
    Q_OBJECT
public:
    explicit WorkshopPlanningDialog(const cad::Part* selectedBody,QWidget* parent=nullptr);
private:
    void evaluateSection();
    void evaluateMachining();
    void evaluateTolerances();
    void exportCurrentReport();
    QString cylinderCsv() const;
    QTabWidget* tabs_=nullptr;
    QComboBox* cylinderPick_=nullptr;
    QComboBox* sectionProfile_=nullptr;
    QDoubleSpinBox *sectionWidth_=nullptr,*sectionHeight_=nullptr,*sectionWall_=nullptr,*sectionFlange_=nullptr,*density_=nullptr;
    QPlainTextEdit *sectionReport_=nullptr,*machiningReport_=nullptr,*toleranceReport_=nullptr,*cylinderReport_=nullptr,*readinessReport_=nullptr;
    QComboBox* machiningOperation_=nullptr;
    QDoubleSpinBox *toolDiameter_=nullptr,*travel_=nullptr,*cutSpeed_=nullptr,*feedRev_=nullptr,
        *feedTooth_=nullptr,*engagementWidth_=nullptr,*cutDepth_=nullptr,*maxRpm_=nullptr,*energy_=nullptr,*machinePower_=nullptr;
    QSpinBox *teeth_=nullptr,*passes_=nullptr;
    QTableWidget* toleranceTable_=nullptr;
    cad::workshop::CylinderInventory cylinders_;
    QString bodyName_;
};
