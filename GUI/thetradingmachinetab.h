#ifndef THETRADINGMACHINETAB_H
#define THETRADINGMACHINETAB_H

#include <QWidget>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QGridLayout>
#include <QTimer>
#include <memory>
#include <thread>
#include <unordered_map>
#include <list>
#include <chrono>
#include "qcustomplot.h"
#include "../BaseModules/InteractiveBrokersClient/InteractiveBrokersClient.h"
#include "../BaseModules/BaseAlgorithm/PlotData.h"
#include "../BaseModules/CandleMaker/CandleMaker.h"
#include "candleplot.h"
#include "volumeplot.h"

// this is a tab set up for the tab pages in the trading machine
class TheTradingMachineTab : public QWidget
{
    Q_OBJECT
public:
    struct AlgorithmApi
    {
        using PlayAlgorithmFnPtr = int (*)(std::string, std::shared_ptr<PlotData>*, std::shared_ptr<InteractiveBrokersClient>, bool);
        using StopAlgorithmFnPtr = bool (*)(int);

        std::function<int(std::string, std::shared_ptr<PlotData>*, std::shared_ptr<InteractiveBrokersClient>, bool)> playAlgorithm;
        std::function<bool(int)> stopAlgorithm;
    };

    ~TheTradingMachineTab();

    // deleted functions because there is no need to duplicate a tab that looks like another
    TheTradingMachineTab(const TheTradingMachineTab& other) = delete;
    TheTradingMachineTab(const TheTradingMachineTab&& other) = delete;
    TheTradingMachineTab& operator=(const TheTradingMachineTab& other) = delete;

    QString tabName() const;
    bool valid() const;

    TheTradingMachineTab(const QString input, bool liveTrading, const AlgorithmApi &api, std::shared_ptr<InteractiveBrokersClient> client, QWidget *parent);
private:
    // plot items
    QGridLayout *gridLayout_;
    QCustomPlot *plot_;
    QTimer* replotTimer_;
    QString name_;

    //algorithm api
    AlgorithmApi api_;
    int algorithmHandle_;
    std::shared_ptr<InteractiveBrokersClient> client_;
    std::shared_ptr<PlotData> plotData_;

    // using pointers for polymorphism and vectors because
    // we will be indexing into different plots to add user
    // annotations
    std::unordered_map<int, std::shared_ptr<BasePlot>> plots_;

    // candle data
    Candlestick currentCandle_;
    CandleMaker candleMaker_;
    std::vector<double>::size_type lastPlotDataIndex_;
    std::vector<double>::size_type lastAnnotationIndex_;

    //plot scale control
    bool autoScale_;
    bool plotActive_;
    bool valid_;
    int replotCount_;

    decltype(std::chrono::high_resolution_clock::now()) lastTickReceivedTime;

    enum MenuType
    {
        CANDLE_MENU, VOLUME_MENU
    };

private:
    void layoutSetup();
    QString formatTabName(const QString& input);
    void updatePlotNewCandle(const Candlestick &candle);
    void updatePlotReplaceCandle(const Candlestick &candle);

private slots:
    void updatePlot(void);
    void menuShowSlot(QPoint pos);

};

#endif // THETRADINGMACHINETAB_H
