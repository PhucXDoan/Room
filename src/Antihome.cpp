#define STB_IMAGE_IMPLEMENTATION true
#include <stb_image.h>
#include "unified.h"
#include "platform.h"
#include "utilities.cpp"

global constexpr vf2 VIEW_DIM       = { 350.0f, 175.0f };
global constexpr f32 WALL_HEIGHT    = 2.7432f;
global constexpr f32 WALL_THICKNESS = 0.5f;
global constexpr f32 LUCIA_HEIGHT   = 1.4986f;
global constexpr vf2 WALLS[][2]     =
	{
		{ { -1.5f, -1.5f }, {  8.5f, -1.5f } },
		{ { -1.5f,  1.5f }, {  4.5f,  7.5f } },
		{ {  4.5f,  7.5f }, {  8.5f,  1.5f } },
		{ { -1.5f, -1.5f }, { -1.5f,  1.5f } },
		{ {  8.5f, -1.5f }, {  8.5f,  1.5f } }
	};
global constexpr vf2 FLOORS[][2] =
	{
		{ { -1.5f + 3.0f * 0.0f, -1.5f }, { 1.5f + 3.0f * 0.0f, 1.5f } },
		{ { -1.5f + 3.0f * 1.0f, -1.5f }, { 1.5f + 3.0f * 1.0f, 1.5f } },
		{ { -1.5f + 3.0f * 2.0f, -1.5f }, { 1.5f + 3.0f * 2.0f, 1.5f } },
		{ { -1.5f + 3.0f * 3.0f, -1.5f }, { 1.5f + 3.0f * 3.0f, 1.5f } }
	};
global constexpr vf2 CEILINGS[][2] =
	{
		{ { -1.5f + 3.0f * 0.0f, -1.5f }, { 1.5f + 3.0f * 0.0f, 1.5f } },
		{ { -1.5f + 3.0f * 2.0f, -1.5f }, { 1.5f + 3.0f * 2.0f, 1.5f } }
	};

struct State
{
	u32                seed;
	SDL_Surface*       view;

	vf2                lucia_velocity;
	vf2                lucia_position;
	f32                lucia_angle_velocity;
	f32                lucia_angle;
	f32                lucia_fov;
	f32                lucia_head_bob_keytime;

	ColumnMajorTexture wall;
	ColumnMajorTexture floor;
	ColumnMajorTexture ceiling;
};

extern "C" PROTOTYPE_INITIALIZE(initialize)
{
	ASSERT(sizeof(State) <= platform->memory_capacity);
	State* state = reinterpret_cast<State*>(platform->memory);

	*state = {};

	state->lucia_fov = TAU / 3.0f;

	SDL_SetRelativeMouseMode(SDL_TRUE);







	#if 0
	DEBUG_printf(">>>>\n");
	vf2 intersection;
	if (intersect_thick_line_segment(&intersection, { 2.65f, 1.01f }, { 0.07f, -3.17f }, { 2.99f, 3.75f }, { 1.42f, -1.89f }, 0.697f))
	{
		DEBUG_printf("Intersection at (%f, %f).\n", intersection.x, intersection.y);
	}
	else
	{
		DEBUG_printf("No intersection.\n");
	}
	DEBUG_printf("<<<<\n");
	#endif
}

extern "C" PROTOTYPE_BOOT_UP(boot_up)
{
	ASSERT(sizeof(State) <= platform->memory_capacity);
	State* state = reinterpret_cast<State*>(platform->memory);

	// @TODO@ More robustiness needed here.
	state->view =
		SDL_CreateRGBSurface
		(
			0,
			static_cast<i32>(VIEW_DIM.x),
			static_cast<i32>(VIEW_DIM.y),
			32,
			0x000000FF,
			0x0000FF00,
			0x00FF0000,
			0xFF000000
		);

	state->wall    = init_column_major_texture(DATA_DIR "wall.bmp");
	state->floor   = init_column_major_texture(DATA_DIR "floor.bmp");
	state->ceiling = init_column_major_texture(DATA_DIR "ceiling.bmp");
}

extern "C" PROTOTYPE_BOOT_DOWN(boot_down)
{
	ASSERT(sizeof(State) <= platform->memory_capacity);
	State* state = reinterpret_cast<State*>(platform->memory);

	deinit_column_major_texture(&state->ceiling);
	deinit_column_major_texture(&state->floor);
	deinit_column_major_texture(&state->wall);
}

extern "C" PROTOTYPE_UPDATE(update)
{
	State* state = reinterpret_cast<State*>(platform->memory);

	state->lucia_angle_velocity -= platform->cursor_delta.x * 0.25f;
	state->lucia_angle_velocity *= 0.4f;
	state->lucia_angle          += state->lucia_angle_velocity * SECONDS_PER_UPDATE;
	if (state->lucia_angle < 0.0f)
	{
		state->lucia_angle += TAU;
	}
	else if (state->lucia_angle >= TAU)
	{
		state->lucia_angle -= TAU;
	}

	vf2 lucia_move = { 0.0f, 0.0f };
	if (HOLDING(Input::s))
	{
		lucia_move.x -= 1.0f;
	}
	if (HOLDING(Input::w))
	{
		lucia_move.x += 1.0f;
	}
	if (HOLDING(Input::d))
	{
		lucia_move.y -= 1.0f;
	}
	if (HOLDING(Input::a))
	{
		lucia_move.y += 1.0f;
	}

	if (+lucia_move)
	{
		state->lucia_velocity += rotate(normalize(lucia_move), state->lucia_angle) * 2.0f;
	}

	state->lucia_velocity *= HOLDING(Input::shift) ? 0.75f : 0.6f;

	vf2 displacement = state->lucia_velocity * SECONDS_PER_UPDATE;
	FOR_RANGE(4)
	{
		f32    intersection_distance = -1.0f;
		vf2    intersection          = { NAN, NAN };
		vf2    intersection_normal   = { NAN, NAN };
		bool32 is_inside             = false;

		FOR_ELEMS(it, WALLS)
		{
			vf2    probing_intersection;
			vf2    probing_normal;
			bool32 probing_inside;
			if (intersect_thick_line_segment(&probing_intersection, &probing_normal, &probing_inside, state->lucia_position, displacement, (*it)[0], (*it)[1], WALL_THICKNESS))
			{
				f32 probing_distance = norm(probing_intersection - state->lucia_position);

				if (probing_inside)
				{
					if (!is_inside || probing_distance > intersection_distance)
					{
						intersection_distance = probing_distance;
						intersection          = probing_intersection;
						intersection_normal   = probing_normal;
						is_inside             = true;
					}
				}
				else if (!is_inside && (intersection_distance == -1.0f || probing_distance < intersection_distance))
				{
					intersection_distance = probing_distance;
					intersection          = probing_intersection;
					intersection_normal   = probing_normal;
				}
			}
		}

		if (intersection_distance == -1.0f)
		{
			break;
		}
		else
		{
			state->lucia_position = intersection;
			displacement          = dot(state->lucia_position + displacement - intersection, { -intersection_normal.y, intersection_normal.x }) * vf2 { -intersection_normal.y, intersection_normal.x };
		}
	}

	state->lucia_position += displacement;

	state->lucia_head_bob_keytime += 0.5f * norm(state->lucia_velocity) * SECONDS_PER_UPDATE;
	if (state->lucia_head_bob_keytime > 1.0f)
	{
		state->lucia_head_bob_keytime -= 1.0f;
	}

	state->lucia_fov += platform->scroll * 0.1f;

	return UpdateCode::resume;
}

extern "C" PROTOTYPE_RENDER(render)
{
	State* state = reinterpret_cast<State*>(platform->memory);

	constexpr i32 VIEW_PADDING = 10;

	fill(platform->surface, { 0.0f, 0.0f, 0.0f, 1.0f });
	fill(state->view      , { 0.1f, 0.2f, 0.3f, 1.0f });

	#if 1
	constexpr f32 MAGIC_K = 0.927295218f * VIEW_DIM.x;
	f32 lucia_eye_level = LUCIA_HEIGHT + 0.025f * (cosf(state->lucia_head_bob_keytime * TAU) - 1.0f);

	FOR_RANGE(x, VIEW_DIM.x)
	{
		vf2 ray_horizontal = polar(state->lucia_angle + (0.5f - x / VIEW_DIM.x) * state->lucia_fov);

		FOR_RANGE(y, 0, static_cast<i32>(VIEW_DIM.y / 2.0f))
		{
			f32 pitch            = (VIEW_DIM.y / 2.0f - y) * state->lucia_fov / MAGIC_K;
			i32 ceiling_index    = -1;
			f32 ceiling_distance = NAN;
			vf2 ceiling_portion  = { NAN, NAN };

			FOR_ELEMS(it, CEILINGS)
			{
				vf2 ortho_distance;
				vf2 ortho_portion;
				if
				(
					ray_cast_line_segment(&ortho_distance.x, &ortho_portion.x, { state->lucia_position.x, lucia_eye_level }, normalize(vf2 { ray_horizontal.x, pitch }), { (*it)[0].x, WALL_HEIGHT }, { (*it)[1].x, WALL_HEIGHT }) &&
					ray_cast_line_segment(&ortho_distance.y, &ortho_portion.y, { state->lucia_position.y, lucia_eye_level }, normalize(vf2 { ray_horizontal.y, pitch }), { (*it)[0].y, WALL_HEIGHT }, { (*it)[1].y, WALL_HEIGHT })
				)
				{
					f32 distance = sqrtf(square(ortho_distance.x) + square(ortho_distance.y) - square(lucia_eye_level));
					if (ceiling_index == -1 || distance < ceiling_distance)
					{
						ceiling_index    = it_index;
						ceiling_distance = distance;
						ceiling_portion  = ortho_portion;
					}
				}
			}

			if (ceiling_index != -1)
			{
				*(reinterpret_cast<u32*>(state->view->pixels) + y * state->view->w + x) =
					to_pixel
					(
						state->view,
						*(state->ceiling.colors + static_cast<i32>(ceiling_portion.x * (state->ceiling.w - 1.0f)) * state->ceiling.h + static_cast<i32>(ceiling_portion.y * state->ceiling.h))
					);
			}
		}
		FOR_RANGE(y, static_cast<i32>(VIEW_DIM.y / 2.0f), VIEW_DIM.y)
		{
			f32 pitch          = (VIEW_DIM.y / 2.0f - y) * state->lucia_fov / MAGIC_K;
			i32 floor_index    = -1;
			f32 floor_distance = NAN;
			vf2 floor_portion  = { NAN, NAN };

			FOR_ELEMS(it, FLOORS)
			{
				vf2 ortho_distance;
				vf2 ortho_portion;
				if
				(
					ray_cast_line_segment(&ortho_distance.x, &ortho_portion.x, { state->lucia_position.x, lucia_eye_level }, normalize(vf2 { ray_horizontal.x, pitch }), { (*it)[0].x, 0.0f }, { (*it)[1].x, 0.0f }) &&
					ray_cast_line_segment(&ortho_distance.y, &ortho_portion.y, { state->lucia_position.y, lucia_eye_level }, normalize(vf2 { ray_horizontal.y, pitch }), { (*it)[0].y, 0.0f }, { (*it)[1].y, 0.0f })
				)
				{
					f32 distance = sqrtf(square(ortho_distance.x) + square(ortho_distance.y) - square(lucia_eye_level));
					if (floor_index == -1 || distance < floor_distance)
					{
						floor_index    = it_index;
						floor_distance = distance;
						floor_portion  = ortho_portion;
					}
				}
			}

			if (floor_index != -1)
			{
				*(reinterpret_cast<u32*>(state->view->pixels) + y * state->view->w + x) =
					to_pixel
					(
						state->view,
						*(state->floor.colors + static_cast<i32>(floor_portion.x * (state->floor.w - 1.0f)) * state->floor.h + static_cast<i32>(floor_portion.y * state->floor.h))
					);
			}
		}

		i32 wall_index    = -1;
		f32 wall_distance = NAN;
		f32 wall_portion  = NAN;

		FOR_ELEMS(it, WALLS)
		{
			f32 distance;
			f32 portion;
			if (ray_cast_line_segment(&distance, &portion, state->lucia_position, ray_horizontal, (*it)[0], (*it)[1]) && (wall_index == -1 || distance < wall_distance))
			{
				wall_index    = it_index;
				wall_distance = distance;
				wall_portion  = portion;
			}
		}

		if (wall_index != -1)
		{
			i32 starting_y = static_cast<i32>(VIEW_DIM.y / 2.0f - MAGIC_K / state->lucia_fov * (WALL_HEIGHT - lucia_eye_level) / wall_distance);
			i32 ending_y   = static_cast<i32>(VIEW_DIM.y / 2.0f + MAGIC_K / state->lucia_fov * lucia_eye_level / wall_distance);

			vf4* texture_column = &state->wall.colors[static_cast<i32>(wall_portion * (state->wall.w - 1.0f)) * state->wall.h];
			FOR_RANGE(y, MAXIMUM(0, starting_y), MINIMUM(ending_y, VIEW_DIM.y))
			{
				*(reinterpret_cast<u32*>(state->view->pixels) + y * state->view->w + x) =
					to_pixel
					(
						state->view,
						texture_column[static_cast<i32>(static_cast<f32>(ending_y - y) / (ending_y - starting_y) * state->wall.h)]
					);
			}
		}
	}
	#else
	#define CONJUGATE(V) (vf2 { (V).x, -(V).y })

	const f32 PIXELS_PER_METER = 25.0f + 10.0f / state->lucia_fov;
	const vf2 ORIGIN           = state->lucia_position;

	FOR_ELEMS(it, WALLS)
	{
		const vf2 DIRECTION  = normalize((*it)[1] - (*it)[0]);
		const vf2 VERTICES[] =
			{
				(*it)[0] + (-DIRECTION + vf2 {  DIRECTION.y, -DIRECTION.x }) * WALL_THICKNESS,
				(*it)[1] + ( DIRECTION + vf2 {  DIRECTION.y, -DIRECTION.x }) * WALL_THICKNESS,
				(*it)[1] + ( DIRECTION + vf2 { -DIRECTION.y,  DIRECTION.x }) * WALL_THICKNESS,
				(*it)[0] + (-DIRECTION + vf2 { -DIRECTION.y,  DIRECTION.x }) * WALL_THICKNESS
			};

		draw_line
		(
			state->view,
			VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + (*it)[0]) * PIXELS_PER_METER,
			VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + (*it)[1]) * PIXELS_PER_METER,
			{ 1.0f, 1.0f, 1.0f, 1.0f }
		);

		FOR_RANGE(i, 4)
		{
			draw_line
			(
				state->view,
				VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + VERTICES[i]          ) * PIXELS_PER_METER,
				VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + VERTICES[(i + 1) % 4]) * PIXELS_PER_METER,
				{ 1.0f, 0.9f, 0.4f, 1.0f }
			);
		}
	}

	constexpr f32 LUCIA_DIM = 4.0f;
	fill(state->view, VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + state->lucia_position) * PIXELS_PER_METER - vf2 { LUCIA_DIM, LUCIA_DIM } / 2.0f, vf2 { LUCIA_DIM, LUCIA_DIM }, { 0.8f, 0.4f, 0.6f, 1.0f });
	draw_line
	(
		state->view,
		VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + state->lucia_position                                   ) * PIXELS_PER_METER,
		VIEW_DIM / 2.0f + CONJUGATE(-ORIGIN + state->lucia_position + polar(state->lucia_angle) * 1.0f) * PIXELS_PER_METER,
		{ 0.4f, 0.8f, 0.6f, 1.0f }
	);
	#endif

	SDL_Rect dst = { static_cast<i32>(VIEW_PADDING), static_cast<i32>(VIEW_PADDING), static_cast<i32>(WIN_DIM.x - VIEW_PADDING * 2.0f), static_cast<i32>((WIN_DIM.x - VIEW_PADDING * 2.0f) * VIEW_DIM.y / VIEW_DIM.x) };
	SDL_BlitScaled(state->view, 0, platform->surface, &dst);
}
