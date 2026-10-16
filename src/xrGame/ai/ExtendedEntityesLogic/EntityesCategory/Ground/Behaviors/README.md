# EntityesCategory/Ground/Behaviors

Общие приёмы наземных схем и единственный менеджер их переключений. Все схемы соблюдают IELBehavior: Enter, Update, Validate, Recover, Exit.

Планируемые файлы:

- `CELGroundBehaviorBase.h/.cpp` — реализует IELBehavior; [ENTITY].
- `CELGroundBehaviorManager.h/.cpp` — выбор и переключение схем; [ENTITY].

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../../../doc/extended-entityes-architecture.md).
