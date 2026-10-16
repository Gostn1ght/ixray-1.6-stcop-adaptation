# ExtendedEntityesLogic

Единая область расширенной логики мутантов. Здесь находятся подключение к движку, компоненты, общие службы и реализации категорий.

Планируемые файлы:

- `EntryPoint.h/.cpp` — жизненный цикл и подключение стадий; [MAIN/WORKER отдельно].
- `CELSharedSettings.h` — статический доступ; загрузка [MAIN], чтение [READ].
- `CELSharedPerceptionBudget.h/.cpp` — реализует IELBudget; общая служба; [ENTITY].
- `CELBaseAdapter.h/.cpp` — реализует IELAdapter; общая база адаптеров владельца; [ENTITY].

Подкаталоги: `CoreComponent/`, `Navigation/`, `Perceptions/`, `Interfaces/`, `Debug/`, `EntityesCategory/`.

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../doc/extended-entityes-architecture.md).
