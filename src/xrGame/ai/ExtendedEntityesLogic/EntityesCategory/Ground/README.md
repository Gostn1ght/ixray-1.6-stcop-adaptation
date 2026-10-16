# EntityesCategory/Ground

Категория наземных компонентов расширения. Использует штатных монстров через адаптеры; общие память и сарафан остаются в корневых службах.

Планируемые файлы:

- `CELGround.h/.cpp` — наследует CELComponentBase; [ENTITY].
- `CELGroundAdapter.h/.cpp` — наследует CELBaseAdapter; наземные возможности; [ENTITY].
- `CELGroundSettings.h` — неизменяемые параметры и квота; [INIT/READ].
- `CELGroundMovement.h/.cpp` — реализует IELMovement; [ENTITY].
- `CELGroundNavigation.h/.cpp` — наследует CELNavigationBase; [REQUEST/WORKER].

Подкаталоги: `Behaviors/`, `Entityes/`.

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../../doc/extended-entityes-architecture.md).
