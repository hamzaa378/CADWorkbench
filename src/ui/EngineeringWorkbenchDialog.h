#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;

struct EngineeringBodyContext {
    QString name;
    double xSizeMm = 0.0;
    double ySizeMm = 0.0;
    double zSizeMm = 0.0;
    double volumeMm3 = 0.0;
    double assignedDensityKgPerM3 = 0.0;
    bool valid = false;
};

// A non-certifying, explicit-units preliminary-design tool. The mathematical
// engine is Qt-independent and has a separate standalone regression suite.
class EngineeringWorkbenchDialog final : public QDialog {
public:
    explicit EngineeringWorkbenchDialog(EngineeringBodyContext body, QWidget* parent = nullptr);

private:
    void filterTools();
    void showSelectedTool();
    void updateCalculation();
    void fillFromSelectedBody();
    QString reportText() const;

    EngineeringBodyContext body_;
    QLineEdit* search_ = nullptr;
    QListWidget* tools_ = nullptr;
    QLabel* description_ = nullptr;
    QLabel* result_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    QFormLayout* form_ = nullptr;
    QPlainTextEdit* notes_ = nullptr;
    std::vector<QDoubleSpinBox*> values_;
    int currentToolIndex_ = -1;
};
