# Navigation

Общая инфраструктура расчётов маршрутов для всех категорий. Исполнение движения принадлежит самим сущностям и их IELMovement.

Планируемые файлы:

- `CELNavigationBase.h/.cpp` — реализует IELNavigation; общий запрос/отмена/прогресс; [REQUEST/WORKER].
- `CELSharedNavigationBudget.h/.cpp` — реализует IELBudget; общий лимит расчётов; [WORKER].
- `CELSharedNavigationScheduler.h/.cpp` — очереди, квоты и порционное выполнение; [WORKER].

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../doc/extended-entityes-architecture.md).
