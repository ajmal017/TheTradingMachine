#ifndef THETRADINGMACHINEMAINWINDOW_H
#define THETRADINGMACHINEMAINWINDOW_H

// Qt
#include <QMainWindow>

// STL
#include <string>
#include <functional>
#include <unordered_set>

// Windows
#include <Windows.h>

// The Trading Machine
#include "TheTradingMachine.h"
#include "SupportBreakShort/SupportBreakShortPlotData.h"
#include "thetradingmachinetabs.h"


namespace Ui {
class TheTradingMachineMainWindow;
}

class TheTradingMachineMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TheTradingMachineMainWindow(QWidget *parent = nullptr);
    ~TheTradingMachineMainWindow();
    bool valid();

private slots:
    void newSession();
    void play();
    void stopCurrentSession();
    void connectInteractiveBroker();
    void closeAll();


//members
private:
    Ui::TheTradingMachineMainWindow *ui;

    // IB Connection. Only one allowed for all sessions
    static IBInterfaceClient* ibInterface_;
    static std::unordered_set<std::wstring> algorithmInstances_;

    // dll interface
    std::wstring dllFile_;
    HMODULE dllHndl_;
    std::function<int(std::string, IBInterfaceClient*)> playAlgorithm;
    std::function<bool(int, SupportBreakShortPlotData::PlotData**)> getPlotData;
    std::function<bool(int)> stopAlgorithm;

    bool valid_;

    void connectDefaulSlots();

//functions
    bool promptLoadAlgorithm();
};

#endif // THETRADINGMACHINEMAINWINDOW_H