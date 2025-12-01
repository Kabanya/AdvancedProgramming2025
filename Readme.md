[![Link to Optick snap](https://img.shields.io/badge/Link%20to%20Optick%20snap-Google%20Drive-blue?style=for-the-badge&logo=googledrive)](https://drive.google.com/drive/folders/1iDs8fLch0lBFlkaebX_IE2XNK5gtf_fb?usp=sharing)

# How To Build

Download SDL3-devel-3.2.22-VC.zip
here https://github.com/libsdl-org/SDL/releases

Unpack into C:\libs
Rename SDL3-3.2.22 to SDL3
Copy "C:\libs\SDL3\lib\x64\SDL3.dll" to AdvancedProgramming/PATH_TO_EXE for Debug and RelWithDebInfo(just a Release below in the text)

Run:
cmake -S . -B build -DSDL3_DIR=\"C:/libs/SDL3/cmake\" && cmake --build build --config Debug
Or
cmake -S . -B build -DSDL3_DIR=\"C:/libs/SDL3/cmake\" && cmake --build build --config RelWithDebInfo

# Tasks

## 0. Setup environment and build

Build the builds and run them successfully.

## 1. Profile it!

Снимите профайл для Debug и Release билдов и сохраните их.
Используйте Optick profiler https://github.com/bombomby/optick
Для семплирований коллстеков запускайте VS/VSCode с правами администратора!

![Profile example](readme_images/profile.jpg)

## 2. Data oriented design refactor

### Before refactoring
#### Debug
![Debug. Before refactoring](./readme_images/screen_capture_before_refactor_debug.png)
#### Release
![Release. Before refactoring](./readme_images/screen_capture_before_refactor_release.png)

### After refactoring
#### Debug
![Debug. After refactoring](./readme_images/screen_capture_after_refactor_debug.png)
#### Release
![Release. After refactoring](./readme_images/screen_capture_after_refactor_release.png)


**DeadLine** - 13.11.25
**Баллы** - 5

Нужно отрефакторить OOP-style код из такого:
```
auto newCell = world.create_object();
newCell->add_component<Sprite>(tileset.get_tile(spriteName));
newCell->add_component<Transform2D>(j, i);
newCell->add_component<BackGroundTag>();
```

В такой при использовании data-ориентированного SoA подхода:
```
struct TilesArchetype
{
    std::vector<Sprite> sprite;
    std::vector<Transform2D> transform;
    // no need to add BackGroundTag, because we don't need it in SoA approach
} tiles;
...
// world.tiles.reserve(n);
world.tiles.sprite.push_back(tileset.get_tile(spriteName));
world.tiles.transform.push_back(Transform2D(j, i));
```

После успешного рефактора снимите профайл "после"

Разбалловка
- 3 балла. Полностью убрано использование класса Component и GameObject.
- 1 балл. Убрана виртуальность в логике food классов и заменена, для примера, на std::variant
- 1 балл. Все игровые объекты разделены на SoA архетипы как в примере. Singleton классы типа StarvationSystem не нужно переделывать под SoA.

Комментарии:
1) класс Enemy - в действительности это просто NPC
2) все удаления сущностей должны быть отложенными (см World::update())
2) для работы с архетипами реализуйте хелперы для добавления/удаления сущностей - OK использовать макросы


## 3. Pathfinding, behaviour tree

**DeadLine** - 20.11.25
**Баллы** - 4


Разбалловка
- 1 балл. Реализован A* для нахождения кратчайщего пути от одной клетки к другой. Он должен принимать опциональный фильтр для отбраковки клеток (чтобы травоядные не прокладывали путь через жищников)
- 1 балл. Реализован FSM для всех AI-агентов. Логика травоядных может быть взята [из лекции](https://towardsdatascience.com/hierarchical-finite-state-machine-for-ai-acting-engine-9b24efc66f2/) Pacman HFSMs. Для хищника FSM будет проще (не нужно спасаться от хищника).

- 1 балл. Реализуйте так же Behaviour Tree, которое полностью повторяло бы поведение для FSM.
- 1 балл. Добавьте новое поведение - размножение. Когда агент сыт (здоровье > 90), он может начать искать другого агента. При попадании в одну клетку с парой появляется новый агент (травоядный или хищник), каждый из родителей передает по 1/3 своего здоровья.

## 4. Task system, thread-pool

**DeadLine** - 4.12.25
**Баллы** - 4

Разбалловка
- 1 балл. Разбейте обновление мира на N функций. Обеспечьте потокобезопасность каждой из них с помощью глобального std::mutex g_worldMutex (см. пример 1 ниже). Добавьте вариант обновления мира world_update_threaded, который будет создавать по треду на каждую функцию обновления (см. пример 2 ниже). Прикрепите скриншоты или .opt файлы debug билда (release опционально) до и после. Это задание исследует какой оверхед создает создание новых потоков на практике

```cpp
extern std::mutex g_worldMutex;
void update_hero_input(World &world, float dt);

void update_hero_input_ts(World& world, float dt)
{
    std::lock_guard<std::mutex> lock(g_worldMutex);
    update_hero_input(world, dt);
}
```

```cpp
void world_update_threaded(World& world, float dt)
{
    std::thread t([&]() {
        hero_input_system(world, dt);
    });
    ...
    t.join();
}
```

- 1 балл. Реализовать (взять готовый) thread pool и реализовать world_update_thread_pool, который будет использовать пул потоков вместо создания новых тредов каждый кадр. Прикрепите скриншоты или .opt файлы debug билда (release опционально) до и после. Целью данного задания является проверить на практике эффективность пула потоков.

```cpp
void world_update_thread_pool(World& world, float dt)
{
    g_threadPool.add_task([&]() {
        hero_input_system(world, dt);
    });
    ...
    g_threadPool.wait_all_task_done();
}
```

- 1 балл. сравнение mutex vs spinlock. Реализуйте сами или [возьмите](https://gist.github.com/glampert/744b8b4d76350e71eed9d0b3b98b0b1d) реализацию spinlock'а и замените использование std::mutex на ваш spinlock. Прикрепите скриншоты или .opt файлы debug билда (release опционально) до и после. Цель задания сравнить мьютекс, который может переходить в сон со spinlock'ом, будет ли видна разница на нашей демке.

- 1 балл. Реализуйте регенерацию мира по клавише R. В качестве синхронизации используйте RCU подход. Но только вместо копирования мира просто сгенерируйте новый

## 5. Network application

**DeadLine** - 11.12.25
**Баллы** - 5
