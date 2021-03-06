#include <fountain/fountaindef.h>
#include <fountain/ft_scene.h>

using ftScene::Scene;
using ftScene::SceneSelector;

namespace fountain {
SceneSelector sceneSelector(&fountain::mainClock);
}

bool ftScene::init()
{
	fountain::sceneSelector.mainClock.init();
	return true;
}

void ftScene::close()
{
}

//class ftScene::Scene
Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::baseInit()
{
	mainClock = ftTime::Clock(&fountain::sceneSelector.mainClock);
	mainClock.init();
}

void Scene::init()
{
}

void Scene::baseUpdate()
{
	mainClock.tick();
}

void Scene::update()
{
}

void Scene::draw()
{
}

void Scene::destroy()
{
}

void Scene::pause()
{
	mainClock.pause();
	isPause = true;
}

void Scene::resume()
{
	mainClock.resume();
	isPause = false;
}

//class ftScene::SceneSelector
SceneSelector::SceneSelector(ftTime::Clock *masterClock)
{
	curScene = NULL;
	oldScene = NULL;
	destroyOldScene = false;
	mainClock = ftTime::Clock(masterClock);
}

void SceneSelector::update()
{
	mainClock.tick();
	if (curScene != NULL) {
		curScene->baseUpdate();
		curScene->update();
	}
	else {
		//TODO:add debug info output
	}
}

void SceneSelector::draw()
{
	if (curScene != NULL) curScene->draw();
	else {
		//TODO:add debug info output
	}
}

void SceneSelector::doAll()
{
	update();
	if (destroyOldScene && oldScene != NULL) {
		delete oldScene;
		oldScene = NULL;
		destroyOldScene = false;
	}
	draw();
}

void SceneSelector::gotoScene(Scene *nextScene, int animeSceneIndex, bool destroyCurScene)
{
	if (curScene != NULL && destroyCurScene) {
		curScene->destroy();
		oldScene = curScene;
		destroyOldScene = true;
	}
	curScene = nextScene;
	//TODO: to init or not
	if (curScene != NULL) {
		curScene->baseInit();
		curScene->init();
		curScene->update();
	}
}
