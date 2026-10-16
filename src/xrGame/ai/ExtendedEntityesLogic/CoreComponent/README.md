# CoreComponent

Единый конвейер подключения расширенной логики к игровым монстрам любых категорий.

Планируемые файлы:

- `CELComponentBase.h/.cpp` — реализует IELLogic; база компонентов расширения; НЕ CBaseMonster; [ENTITY].
- `CELSharedFactory.h/.cpp` — выбор компонента и профиля внутри модуля; [ENTITY].
- `TExtendedEntityesLogic.h/.cpp` — ECS; каждый вызов через GetComponent, указатель НЕ хранить; внутри xr_unique_ptr<IELLogic>; [ENTITY].

Статус: проектирование; C++-файлы пока только запланированы. Контракты и правила: [план архитектуры](../../../../../doc/extended-entityes-architecture.md).
