# EntityesCategory/Flying/Navigation

Расчёт и проверка трёхмерного коридора с учётом тела, поверхности и границ уровня. Действующий маршрут сохраняется до принятия пригодной замены; исполнение принадлежит Movement.

Планируемые файлы:

- `CELFlyingNavigation.h/.cpp` — наследует CELNavigationBase; [REQUEST/WORKER].
- `CELFlyingGeometry.h/.cpp` — проверки маршрута; [WORKER после аудита].

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../../../doc/extended-entityes-architecture.md).
