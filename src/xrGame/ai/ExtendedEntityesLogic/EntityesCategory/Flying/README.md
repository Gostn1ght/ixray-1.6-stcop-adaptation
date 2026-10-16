# EntityesCategory/Flying

Категория полноценных летунов. Игровая база наследует CBaseMonster; отдельная компонентная база управляет расширенным поведением. Зависимость от тушкана и имитация полёта поднятым shape не переносятся.

Планируемые файлы:

- `CELFlying.h/.cpp` — наследует CELComponentBase; компонент категории; [ENTITY].
- `CELFlyingAdapter.h/.cpp` — наследует CELBaseAdapter; воздушные возможности; [ENTITY].
- `CELFlyingEntityBase.h/.cpp` — наследует CBaseMonster; игровая база новых летунов; [ENTITY].
- `CELFlyingSettings.h` — неизменяемые параметры и квота; [INIT/READ].
- `CELFlyingMovement.h/.cpp` — реализует IELMovement; [ENTITY].

Подкаталоги: `Behaviors/`, `Navigation/`, `Entityes/`.

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../../doc/extended-entityes-architecture.md).
