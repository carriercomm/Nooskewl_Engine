#ifndef MAP_H
#define MAP_H

#include "Nooskewl_Engine/a_star.h"
#include "Nooskewl_Engine/main.h"
#include "Nooskewl_Engine/map_entity.h"
#include "Nooskewl_Engine/map_logic.h"
#include "Nooskewl_Engine/speech.h"
#include "Nooskewl_Engine/tilemap.h"
#include "Nooskewl_Engine/types.h"

namespace Nooskewl_Engine {

class NOOSKEWL_ENGINE_EXPORT Map {
public:
	Map(std::string map_name);
	~Map();

	void handle_event(TGUI_Event *event);
	void update_camera();
	bool update();
	void draw();

	void add_entity(Map_Entity *entity);
	void add_speech(std::string text);
	void change_map(std::string map_name, Point<int> position, Direction direction);

	bool is_solid(int layer, Point<int> position, Size<int> size);
	void check_triggers(Map_Entity *entity);
	void get_new_map_details(std::string &map_name, Point<int> &position, Direction &direction);
	Map_Entity *get_entity(int id);
	std::string get_map_name();
	Tilemap *get_tilemap();
	Point<int> get_offset();
	std::list<A_Star::Node *> find_path(Point<int> start, Point<int> goal);

private:
	Tilemap *tilemap;
	Point<int> offset;
	std::vector<Map_Entity *> entities;

	std::vector<std::string> speeches;
	Speech *speech;

	std::string map_name;
	std::string new_map_name;
	Point<int> new_map_position;
	Direction new_map_direction;

	Map_Logic *ml;

	A_Star *a_star;
};

} // End namespace Nooskewl_Engine

#endif // MAP_H
