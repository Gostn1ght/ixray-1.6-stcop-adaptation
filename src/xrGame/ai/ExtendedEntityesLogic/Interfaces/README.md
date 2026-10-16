# Interfaces

Единые контракты подключения категорий и их внутренних механизмов. Здесь объявляются интерфейсы и необходимые компактные типы результатов; алгоритмов и хранилищ здесь нет.

Планируемые файлы:

- `IELLogic.h` — интерфейс логики; CELComponentBase и его наследники категорий.
- `IELBehavior.h` — интерфейс схем; базы поведений категорий и их наследники.
- `IELNavigation.h` — интерфейс навигации; CELNavigationBase и его наследники.
- `IELMovement.h` — интерфейс движения; CELGroundMovement, CELFlyingMovement.
- `IELBudget.h` — интерфейс бюджета; CELSharedPerceptionBudget, CELSharedNavigationBudget.
- `IELAdapter.h` — интерфейс адаптера; CELBaseAdapter и его наследники категорий.

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../doc/extended-entityes-architecture.md).
