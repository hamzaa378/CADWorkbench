#pragma once
#include "EngineeringWorkbenchDialog.h"
#include <QDialog>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;

// Qt wrapper around the independently tested, non-certifying workflow engine.
class GuidedStudyDialog final : public QDialog {
public:
    explicit GuidedStudyDialog(EngineeringBodyContext body,QWidget* parent=nullptr);
private:
    void chooseStudy();
    void refreshResults();
    void loadBodyDimensions();
    QString report() const;
    EngineeringBodyContext body_;
    QComboBox* studies_=nullptr;
    QFormLayout* form_=nullptr;
    QPlainTextEdit* results_=nullptr;
    QLabel* assumptions_=nullptr;
    std::vector<QDoubleSpinBox*> fields_;
};
