#include <QRadioButton>
#include "ValueRouter.h"
#include "ValuablePlanetRouteViewer.h"
#include "MainWindow.h"


ValueRouter::~ValueRouter() {
}

ValueRouter::ValueRouter(QWidget *parent, AStarRouter *router, SystemList *systems)
        : AbstractBaseWindow(parent, router, systems) {
    _systemsOnly = true;
    scanJournals();
    connect(_ui->rescanJournalButton, SIGNAL(clicked()), this, SLOT(scanJournals()));
    connect(_ui->filterCommander, SIGNAL(currentTextChanged(const QString &)), this, SLOT(updateSystem()));
    updateFilters();
}

void ValueRouter::scanJournals() {
    _commanderExploredSystems.clear();
    _commanderInformation.clear();
    QDir          dir(MainWindow::journalDirectory(), "Journal.*.log");
    QFileInfoList list        = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files, QDir::Time | QDir::Reversed);

    for(auto entry: list) {
        auto file = entry.absoluteFilePath();
        JournalFile journalFile(file);
        connect(&journalFile, SIGNAL(onEvent(const JournalFile &, const Event &)), this, SLOT(handleEvent(const JournalFile &, const Event &)));
        journalFile.parse();
    }
    const auto comboBox = _ui->filterCommander;
    comboBox->clear();
    comboBox->addItems(_commanderExploredSystems.keys());
    updateCommanderAndSystem();
    updateFilters();
}

void ValueRouter::updateFilters() {
    QList<int8_t> typeFilter;

    typeFilter.append((int8_t) (_ui->elw->isChecked() ? 1 : 0));
    typeFilter.append((int8_t) (_ui->ww->isChecked() ? 1 : 0));
    typeFilter.append((int8_t) (_ui->wwt->isChecked() ? 1 : 0));
    typeFilter.append((int8_t) (_ui->aw->isChecked() ? 1 : 0));
    typeFilter.append((int8_t) (_ui->tf->isChecked() ? 1 : 0));

    _filteredSystems.clear();

    QSet<QString> *excludedSystems(nullptr);
    const QString &filterCommander = _ui->filterCommander->currentText();
    if(_ui->filterVisited->isChecked() && !filterCommander.isEmpty() &&
       _commanderExploredSystems.contains(filterCommander)) {
        excludedSystems = &_commanderExploredSystems[filterCommander];
    }

    for(const auto &system : _router->systems()) {
        if(system.matchesFilter(typeFilter) && system.estimatedValue() >= _ui->minSystemValue->value() &&
           (!excludedSystems || !excludedSystems->contains(system.name().toUpper()))) {
            _filteredSystems.append(system);
        }
    }
    
    _ui->statusBar->showMessage(QString("Filter matches %1 systems.").arg(_filteredSystems.size()));
}

void ValueRouter::routeCalculated(const RouteResult &route) {
    AbstractBaseWindow::routeCalculated(route);
    auto viewer = new ValuablePlanetRouteViewer(route);
    viewer->show();
}

void ValueRouter::handleEvent(const JournalFile &file, const Event &ev) {
    const QString &commander = file.commander();
    const QString &systemName = file.system();
    if(commander.isEmpty() || systemName.isEmpty()) {
        return;
    }
    switch(ev.type()) {
    case EventTypeLocation:
    case EventTypeFSDJump:
        if(updateCommanderInfo(file, ev, commander)) {
            if(!_commanderExploredSystems.contains(commander)) {
                _commanderExploredSystems[commander] = QSet<QString>();
            }
        }
        break;
    case EventTypeScan:
        _commanderExploredSystems[commander].insert(systemName.toUpper());
        break;
    default:
        break;
    }
}

void ValueRouter::updateSystem() {
    const QString &system = _commanderInformation[_ui->filterCommander->currentText()]._system;
    _ui->systemName->setText(system);
    _systemResolver->resolve(system);
}