# Debug — общая отладка ExtendedEntityesLogic

Общая очередь отладки и консольные режимы; сборку и проверку в игре выполняет пользователь.

Текущие файлы:

- `Debug.h/.cpp` — класс CELSharedDebug; очередь, фильтры и сроки отладки; [MAIN]; потокобезопасная публикация.
- `DebugCommands.h/.cpp` — класс CELSharedDebugCommands; прямая ранняя регистрация из xrRender_initconsole и применение копий режима в BeginFrame; [MAIN].
- `DebugDrawHud.h/.cpp` — класс CELSharedDebugDrawHud; текст и статистика HUD; [MAIN].
- `DebugDrawWorld.h/.cpp` — класс CELSharedDebugDrawWorld; примитивы через HUD().world_prims; [MAIN].

Статус: перенос в процессе, сборку выполняет пользователь. Готовность и оставшиеся механики: [архитектура](../../../../../doc/extended-entityes-architecture.md).

Обработчики в DebugCommands.h не зависят от игрового состояния и используют штатный Save/Status. monster_logic сохраняет on/off/config, draw/stats — on/off; reset статистики не сохраняется как действие. Регистрации при входе на уровень нет. user.ltx вручную не менять.
