#include "Nooskewl_Engine/engine.h"
#include "Nooskewl_Engine/internal.h"
#include "Nooskewl_Engine/tilemap.h"

using namespace Nooskewl_Engine;

Tilemap::Tilemap(std::string map_filename)
{
	map_filename = "maps/" + map_filename;

	for (int i = 0; i < 256; i++) {
		std::string filename = std::string("sheets/tiles" + itos(i) + ".tga");
		Image *image;
		try {
			image = new Image(filename, true);
		}
		catch (Error e) {
			if (i == 0) {
				throw LoadError("no tile sheets!");
			}
			else {
				break;
			}
		}
		sheets.push_back(image);
	}

	SDL_RWops *f;
	try {
		f = open_file(map_filename);
	}
	catch (Error e) {
		for (size_t i = 0; i < sheets.size(); i++) {
			delete sheets[i];
		}
		sheets.clear();
		throw e;
	}

	size.w = SDL_ReadLE16(f);
	size.h = SDL_ReadLE16(f);
	num_layers = SDL_fgetc(f);

	layers = new Layer[num_layers];

	for (int layer = 0; layer < num_layers; layer++) {
		layers[layer].sheets_used = std::vector<int>();
		layers[layer].sheet = new int *[size.h];
		layers[layer].x = new int *[size.h];
		layers[layer].y = new int *[size.h];
		layers[layer].solid = new bool *[size.h];
		for (int row = 0; row < size.h; row++) {
			layers[layer].sheet[row] = new int[size.w];
			layers[layer].x[row] = new int[size.w];
			layers[layer].y[row] = new int[size.w];
			layers[layer].solid[row] = new bool[size.w];
			for (int col = 0; col < size.w; col++) {
				layers[layer].x[row][col] = (char)SDL_fgetc(f);
				layers[layer].y[row][col] = (char)SDL_fgetc(f);
				layers[layer].sheet[row][col] = (char)SDL_fgetc(f);
				layers[layer].solid[row][col] = SDL_fgetc(f) != 0;
				if (layers[layer].sheet[row][col] != 0 && layers[layer].sheet[row][col] != -1) {
				}

				if (layers[layer].x[row][col] >= 0 && std::find(layers[layer].sheets_used.begin(), layers[layer].sheets_used.end(), layers[layer].sheet[row][col]) == layers[layer].sheets_used.end()) {
					layers[layer].sheets_used.push_back(layers[layer].sheet[row][col]);
				}
			}
		}
	}

	SDL_RWclose(f);

	for (int layer = 0; layer < num_layers; layer++) {
		std::sort(layers[layer].sheets_used.begin(), layers[layer].sheets_used.end());
	}
}

Tilemap::~Tilemap()
{
	for (size_t i = 0; i < sheets.size(); i++) {
		delete sheets[i];
	}

	if (layers) {
		for (int layer = 0; layer < num_layers; layer++) {
			for (int row = 0; row < size.h; row++) {
				delete[] layers[layer].sheet[row];
				delete[] layers[layer].x[row];
				delete[] layers[layer].y[row];
				delete[] layers[layer].solid[row];
			}
			delete[] layers[layer].sheet;
			delete[] layers[layer].x;
			delete[] layers[layer].y;
			delete[] layers[layer].solid;
		}

		delete[] layers;
	}
}

int Tilemap::get_num_layers()
{
	return num_layers;
}

Size<int> Tilemap::get_size()
{
	return size;
}

bool Tilemap::is_solid(int layer, Point<int> position)
{
	int start_layer = layer < 0 ? 0 : layer;
	int end_layer = layer < 0 ? num_layers - 1 : layer;

	for (int i = start_layer; i <= end_layer; i++) {
		Layer l = layers[i];
		if (l.solid[position.y][position.x]) {
			return true;
		}
	}

	return false;
}

bool Tilemap::collides(int layer, Point<int> topleft, Point<int> bottomright)
{
	int start_layer = layer < 0 ? 0 : layer;
	int end_layer = layer < 0 ? num_layers - 1 : layer;

	int start_column = topleft.x / noo.tile_size;
	int end_column = bottomright.x / noo.tile_size;
	int start_row = topleft.y / noo.tile_size;
	int end_row = bottomright.y / noo.tile_size;

	start_column = MIN(size.w-1, MAX(0, start_column));
	end_column = MIN(size.w-1, MAX(0, end_column));
	start_row = MIN(size.h-1, MAX(0, start_row));
	end_row = MIN(size.h-1, MAX(0, end_row));

	for (int i = start_layer; i <= end_layer; i++) {
		Layer l = layers[i];

		for (int row = start_row; row <= end_row; row++) {
			for (int column = start_column; column <= end_column; column++) {
				if (l.solid[row][column]) {
					return true;
				}
			}
		}
	}

	return false;
}

void Tilemap::draw(int layer, Point<int> position, bool set_z)
{
	Layer l = layers[layer];

	for (size_t sheet = 0; sheet < l.sheets_used.size(); sheet++) {
		int sheet_num = l.sheets_used[sheet];

		sheets[sheet_num]->start();

		for (int row = 0; row < size.h; row++) {
			for (int col = 0; col < size.w; col++) {
				int s = l.sheet[row][col];
				if (s == sheet_num) {
					int x = l.x[row][col];
					int y = l.y[row][col];
					int sx = x * noo.tile_size;
					int sy = y * noo.tile_size;
					int dx = position.x + col * noo.tile_size;
					int dy = position.y + row * noo.tile_size;
					sheets[s]->draw_region_z(
						Point<int>(sx, sy),
						Size<int>(noo.tile_size, noo.tile_size),
						Point<int>(dx, dy),
						set_z ? -(1.0f-((float)(row * noo.tile_size)/(float)(size.h*noo.tile_size))) : 0.0f,
						0
					);
				}
			}
		}

		sheets[sheet_num]->end();
	}
}
