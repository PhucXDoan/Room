#define PRESSED(INPUT)  (!!platform->inputs[+(INPUT)].curr && !platform->inputs[+(INPUT)].prev)
#define HOLDING(INPUT)  (!!platform->inputs[+(INPUT)].curr)
#define RELEASED(INPUT) (!platform->inputs[+(INPUT)].curr && !!platform->inputs[+(INPUT)].prev)

struct RGBA
{
	union
	{
		struct
		{
			u8 r;
			u8 g;
			u8 b;
			u8 a;
		};

		u32 rgba;
	};
};

struct Image
{
	vi2   dim;
	RGBA* data;
};

struct TextureSprite
{
	Image        image;
	SDL_Texture* texture;
};

struct AnimatedSprite
{
	f32   age_hertz;
	f32   age_keytime;
	i32   current_index;
	i32   frame_count;
	vi2   frame_dim;
	RGBA* data;
};

struct Mipmap
{
	i32   level_count;
	vi2   base_dim;
	RGBA* data;
};

internal bool32 in_rect(vf2 position, vf2 bottom_left, vf2 dimensions)
{
	return
		IN_RANGE(position.x, bottom_left.x, bottom_left.x + dimensions.x) &&
		IN_RANGE(position.y, bottom_left.y, bottom_left.y + dimensions.y);
}

internal bool32 in_rect_centered(vf2 position, vf2 center, vf2 dimensions)
{
	return
		IN_RANGE(position.x, center.x - dimensions.x / 2.0f, center.x + dimensions.x / 2.0f) &&
		IN_RANGE(position.y, center.y - dimensions.y / 2.0f, center.y + dimensions.y / 2.0f);
}

internal constexpr u32 pack_color(vf3 color)
{
	return
		(static_cast<u8>(color.x * 255.0f) << 24) |
		(static_cast<u8>(color.y * 255.0f) << 16) |
		(static_cast<u8>(color.z * 255.0f) <<  8) |
		0xFF;
}

internal constexpr u32 pack_color(vf4 color)
{
	return
		(static_cast<u8>(color.x * 255.0f) << 24) |
		(static_cast<u8>(color.y * 255.0f) << 16) |
		(static_cast<u8>(color.z * 255.0f) <<  8) |
		(static_cast<u8>(color.w * 255.0f) <<  0);
}

internal constexpr vf4 unpack_color(u32 pixel)
{
	return
		{
			static_cast<f32>((pixel >> 24) & 0xFF) / 255.0f,
			static_cast<f32>((pixel >> 16) & 0xFF) / 255.0f,
			static_cast<f32>((pixel >>  8) & 0xFF) / 255.0f,
			static_cast<f32>((pixel >>  0) & 0xFF) / 255.0f
		};
}

internal Image init_image(strlit file_path)
{
	Image image;

	u32* stbimg = reinterpret_cast<u32*>(stbi_load(file_path, &image.dim.x, &image.dim.y, 0, STBI_rgb_alpha));
	DEFER { stbi_image_free(stbimg); };
	ASSERT(stbimg);

	image.data = reinterpret_cast<RGBA*>(malloc(image.dim.x * image.dim.y * sizeof(RGBA)));

	FOR_RANGE(y, image.dim.y)
	{
		FOR_RANGE(x, image.dim.x)
		{
			u32 pixel = stbimg[y * image.dim.x + x];
			image.data[x * image.dim.y + y] = { (pixel >> 0) & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF, (pixel >> 24) & 0xFF };
		}
	}

	return image;
}

internal void deinit_image(Image* image)
{
	free(image->data);
}

internal TextureSprite init_texture_sprite(SDL_Renderer* renderer, strlit file_path)
{
	TextureSprite sprite;

	sprite.image   = init_image(file_path);
	sprite.texture = IMG_LoadTexture(renderer, file_path);
	ASSERT(sprite.texture);

	return sprite;
}

internal void deinit_texture_sprite(TextureSprite* sprite)
{
	deinit_image(&sprite->image);
	SDL_DestroyTexture(sprite->texture);
}

internal AnimatedSprite init_animated_sprite(strlit file_path, vi2 sheet_dim, f32 age_hertz)
{
	vi2  stbdim;
	u32* stbimg = reinterpret_cast<u32*>(stbi_load(file_path, &stbdim.x, &stbdim.y, 0, STBI_rgb_alpha));
	DEFER { stbi_image_free(stbimg); };
	ASSERT(stbimg);

	AnimatedSprite sprite;

	sprite.age_hertz     = age_hertz;
	sprite.age_keytime   = 0.0f;
	sprite.current_index = 0;
	sprite.frame_dim     = { stbdim.x / sheet_dim.x, stbdim.y / sheet_dim.y };
	sprite.data          = reinterpret_cast<RGBA*>(malloc(stbdim.x * stbdim.y * sizeof(RGBA)));
	sprite.frame_count   = sheet_dim.x * sheet_dim.y;

	FOR_RANGE(i, sheet_dim.x * sheet_dim.y)
	{
		FOR_RANGE(y, sprite.frame_dim.y)
		{
			FOR_RANGE(x, sprite.frame_dim.x)
			{
				u32 pixel = stbimg[(i / sheet_dim.x * sprite.frame_dim.y + y) * stbdim.x + (i % sheet_dim.x * sprite.frame_dim.x + x)];
				sprite.data[i * sprite.frame_dim.x * sprite.frame_dim.y + x * sprite.frame_dim.y + y] = { (pixel >> 0) & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF, (pixel >> 24) & 0xFF };
			}
		}
	}

	return sprite;
}

internal void deinit_animated_sprite(AnimatedSprite* sprite)
{
	free(sprite->data);
}

internal Mipmap init_mipmap(strlit file_path, i32 level_count)
{
	ASSERT(IN_RANGE(level_count, 1, 8));

	Mipmap mipmap;
	mipmap.level_count = level_count;

	vi2  stbdim;
	u32* stbimg = reinterpret_cast<u32*>(stbi_load(file_path, &stbdim.x, &stbdim.y, 0, STBI_rgb_alpha));
	DEFER { stbi_image_free(stbimg); };
	ASSERT(stbimg);

	mipmap.base_dim = { stbdim.x * 2 / 3, stbdim.y };
	mipmap.data     = reinterpret_cast<RGBA*>(malloc((mipmap.base_dim.x * mipmap.base_dim.y * 2 - mipmap.base_dim.x * mipmap.base_dim.y * 2 / (1 << level_count)) * sizeof(RGBA)));

	vi2   stbimg_coordinates = { 0, 0 };
	RGBA* mipmap_pixel       = mipmap.data;
	FOR_RANGE(i, level_count)
	{
		FOR_RANGE(ix, mipmap.base_dim.x / (1 << i))
		{
			FOR_RANGE(iy, mipmap.base_dim.y / (1 << i))
			{
				u32 stbimg_pixel = *(stbimg + (stbimg_coordinates.y + iy) * stbdim.x + stbimg_coordinates.x + ix);
				*mipmap_pixel++ =
					{
						static_cast<u8>((stbimg_pixel >>  0) & 0xFF),
						static_cast<u8>((stbimg_pixel >>  8) & 0xFF),
						static_cast<u8>((stbimg_pixel >> 16) & 0xFF),
						static_cast<u8>((stbimg_pixel >> 24) & 0xFF)
					};
			}
		}

		stbimg_coordinates = { mipmap.base_dim.x, mipmap.base_dim.y - mipmap.base_dim.y / (1 << i) };
	}

	return mipmap;
}

internal void deinit_mipmap(Mipmap* mipmap)
{
	free(mipmap->data);
}

internal Image get_image_of_frame(AnimatedSprite* sprite)
{
	Image image;
	image.dim  = sprite->frame_dim;
	image.data = sprite->data + sprite->current_index * sprite->frame_dim.x * sprite->frame_dim.y;
	return image;
}

internal vf4 sample_at(Image* image, vf2 uv)
{
	ASSERT(0.0f <= uv.x && uv.x <= 1.0f);
	ASSERT(0.0f <= uv.y && uv.y <= 1.0f);
	RGBA rgba = image->data[static_cast<i32>(uv.x * (image->dim.x - 1.0f)) * image->dim.y + static_cast<i32>((1.0f - uv.y) * (image->dim.y - 1.0f))];
	return { rgba.r / 255.0f, rgba.g / 255.0f, rgba.b / 255.0f, rgba.a / 255.0f };
}

internal vf3 sample_at(Mipmap* mipmap, f32 level, vf2 uv)
{
#if DEBUG_DISABLE_MIPMAPPING
	level = 0.0f;
#endif

	ASSERT(0.0f <= uv.x && uv.x <= 1.0f);
	ASSERT(0.0f <= uv.y && uv.y <= 1.0f);

	i32 l = static_cast<i32>(clamp(level, 0.0f, mipmap->level_count - 1.0f));
	RGBA p =
		mipmap->data
		[
			mipmap->base_dim.x * mipmap->base_dim.y * 4 / 3 - mipmap->base_dim.x * mipmap->base_dim.y * 4 / 3 / (1 << (l * 2))
			+ static_cast<i32>(uv.x * (mipmap->base_dim.x / (1 << l) - 1.0f)) * (mipmap->base_dim.y / (1 << l))
			+ static_cast<i32>((1.0f - uv.y) * (mipmap->base_dim.y / (1 << l) - 1.0f))
		];

	if (IN_RANGE(level, 0.0f, mipmap->level_count - 1.0f))
	{
		RGBA q =
			mipmap->data
			[
				mipmap->base_dim.x * mipmap->base_dim.y * 4 / 3 - mipmap->base_dim.x * mipmap->base_dim.y * 4 / 3 / (1 << ((l + 1) * 2))
				+ static_cast<i32>(uv.x * (mipmap->base_dim.x / (1 << (l + 1)) - 1.0f)) * (mipmap->base_dim.y / (1 << (l + 1)))
				+ static_cast<i32>((1.0f - uv.y) * (mipmap->base_dim.y / (1 << (l + 1)) - 1.0f))
			];

		return vf3 { lerp(p.r, q.r, level - l), lerp(p.g, q.g, level - l), lerp(p.b, q.b, level - l) } / 255.0f; // @TODO@ Optimize this.
	}
	else
	{
		return vf3 { p.r / 255.0f, p.g / 255.0f, p.b / 255.0f };
	}
}

internal void age_animated_sprite(AnimatedSprite* sprite, f32 delta_time)
{
	sprite->age_keytime += delta_time * sprite->age_hertz;
	while (sprite->age_keytime >= 1.0f)
	{
		sprite->current_index  = (sprite->current_index + static_cast<i32>(sprite->age_keytime)) % sprite->frame_count;
		sprite->age_keytime    -= static_cast<i32>(sprite->age_keytime);
	}
}

internal bool32 ray_cast_line(f32* scalar, f32* portion, vf2 position, vf2 ray, vf2 start, vf2 end)
{
	*scalar = ((start.x - end.x) * (start.y - position.y) - (start.y - end.y) * (start.x - position.x)) / ((start.x - end.x) * ray.y - (start.y - end.y) * ray.x);

	if (*scalar < 0.0f)
	{
		return false;
	}
	else
	{
		f32 portion_c = start.x * ray.y - start.y * ray.x;
		*portion = (portion_c + ray.x * position.y - ray.y * position.x) / (portion_c + ray.x * end.y - ray.y * end.x);

		return true;
	}
}

enum struct Orientation : u8
{
	collinear,
	clockwise,
	counterclockwise
};

bool32 is_point_on_line_segment(vf2 p, vf2 q, vf2 r)
{
	return q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) && q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y);
}

internal Orientation orientation_of(vf2 p, vf2 q, vf2 r)
{
	f32 n = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);

	if (n == 0.0f)
	{
		return Orientation::collinear;
	}
	else if (n > 0.0f)
	{
		return Orientation::clockwise;
	}
	else
	{
		return Orientation::counterclockwise;
	}
}

bool32 is_line_segment_intersecting(vf2 p1, vf2 q1, vf2 p2, vf2 q2)
{
	Orientation o1 = orientation_of(p1, q1, p2);
	Orientation o2 = orientation_of(p1, q1, q2);
	Orientation o3 = orientation_of(p2, q2, p1);
	Orientation o4 = orientation_of(p2, q2, q1);

	return
		o1 != o2 && o3 != o4
		|| o1 == Orientation::collinear && is_point_on_line_segment(p1, p2, q1)
		|| o2 == Orientation::collinear && is_point_on_line_segment(p1, q2, q1)
		|| o3 == Orientation::collinear && is_point_on_line_segment(p2, p1, q2)
		|| o4 == Orientation::collinear && is_point_on_line_segment(p2, q1, q2);
}

struct CollisionData
{
	bool16 exists;
	bool16 inside;
	vf2    displacement;
	vf2    normal;
};

internal CollisionData prioritize_collision(CollisionData a, CollisionData b)
{
	if (a.exists)
	{
		if (b.exists && norm(a.displacement) * (a.inside ? 1.0f : -1.0f) < norm(b.displacement) * (b.inside ? 1.0f : -1.0f))
		{
			return b;
		}
		else
		{
			return a;
		}
	}
	else
	{
		return b;
	}
}

internal CollisionData collide_thick_line(vf2 position, vf2 ray, vf2 start, vf2 end, f32 padding)
{
	vf2 n  = normalize(rotate90(end - start));
	f32 d0 = dot(start + n * padding - position, n);
	f32 d1 = dot(start - n * padding - position, n);

	constexpr f32 EPSILON = 0.001f;
	if (fabsf(d0) <= EPSILON) { d0 = 0.0f; }
	if (fabsf(d1) <= EPSILON) { d1 = 0.0f; }

	if (sign(d0) == sign(d1))
	{
		f32 den = (end.y - start.y) * ray.x - (end.x - start.x) * ray.y;
		f32 k0  = ((end.x - start.x) * (position.y - start.y - n.y * padding) - (end.y - start.y) * (position.x - start.x - n.x * padding)) / den;
		f32 k1  = ((end.x - start.x) * (position.y - start.y + n.y * padding) - (end.y - start.y) * (position.x - start.x + n.x * padding)) / den;

		if (k0 < k1)
		{
			if (0.0f < k0 && k0 <= 1.0f)
			{
				CollisionData data;
				data.exists       = true;
				data.inside       = false;
				data.displacement = k0 * ray;
				data.normal       = n;
				return data;
			}
			else
			{
				CollisionData data;
				data.exists = false;
				return data;
			}
		}
		else if (0.0f < k1 && k1 <= 1.0f)
		{
			CollisionData data;
			data.exists       = true;
			data.inside       = false;
			data.displacement = k1 * ray;
			data.normal       = -n;
			return data;
		}
		else
		{
			CollisionData data;
			data.exists = false;
			return data;
		}
	}
	else if (d0 == 0.0f && dot(ray, n) >= 0.0f || d1 == 0.0f && dot(ray, n) <= 0.0f)
	{
		CollisionData data;
		data.exists = false;
		return data;
	}
	else if (fabsf(d0) < fabsf(d1))
	{
		CollisionData data;
		data.exists       = true;
		data.inside       = true;
		data.displacement = d0 * n;
		data.normal       = n;
		return data;
	}
	else
	{
		CollisionData data;
		data.exists       = true;
		data.inside       = true;
		data.displacement = d1 * n;
		data.normal       = -n;
		return data;
	}
}

internal CollisionData collide_circle(vf2 position, vf2 ray, vf2 center, f32 radius)
{
	f32 norm_sq_ray  = norm_sq(ray);
	f32 discriminant = norm_sq_ray * radius * radius - square(ray.x * (position.y - center.y) - ray.y * (position.x - center.x));

	if (discriminant <= 0.0f)
	{
		CollisionData data;
		data.exists = false;
		return data;
	}
	else
	{
		f32 k0 = (dot(center - position, ray) - sqrtf(discriminant)) / norm_sq_ray;
		f32 k1 = (dot(center - position, ray) + sqrtf(discriminant)) / norm_sq_ray;

		if (k0 < 0.0f && k1 < 0.0f || dot(position - center, ray) >= 0.0f)
		{
			CollisionData data;
			data.exists = false;
			return data;
		}
		else if (sign(k0) == sign(k1))
		{
			if (min(k0, k1) <= 1.0f)
			{
				CollisionData data;
				data.exists       = true;
				data.inside       = false;
				data.displacement = min(k0, k1) * ray;
				data.normal       = normalize(position + data.displacement - center);
				return data;
			}
			else
			{
				CollisionData data;
				data.exists = false;
				return data;
			}
		}
		else
		{
			CollisionData data;
			data.exists       = true;
			data.inside       = true;
			data.displacement = normalize(position - center) * radius + center - position;
			data.normal       = data.displacement + position - center;
			return data;
		}
	}
}

internal CollisionData collide_pill(vf2 position, vf2 ray, vf2 start, vf2 end, f32 padding)
{
	CollisionData data = collide_thick_line(position, ray, start, end, padding);

	if (data.exists)
	{
		f32 portion = dot(position + data.displacement - start, (end - start) / norm_sq(end - start));
		data.exists = 0.0f <= portion && portion <= 1.0f;
	}

	return prioritize_collision(prioritize_collision(data, collide_circle(position, ray, start, padding)), collide_circle(position, ray, end, padding));
}

internal void set_color(SDL_Renderer* renderer, vf3 color)
{
	SDL_SetRenderDrawColor
	(
		renderer,
		static_cast<u8>(color.x * 255.0f),
		static_cast<u8>(color.y * 255.0f),
		static_cast<u8>(color.z * 255.0f),
		255
	);
}

internal void set_color(SDL_Renderer* renderer, vf4 color)
{
	SDL_SetRenderDrawColor
	(
		renderer,
		static_cast<u8>(color.x * 255.0f),
		static_cast<u8>(color.y * 255.0f),
		static_cast<u8>(color.z * 255.0f),
		static_cast<u8>(color.w * 255.0f)
	);
}

internal void render_circle(SDL_Renderer* renderer, vf2 center, f32 radius)
{
	ASSERT(radius < 256.0f);

	vf2 p     = { radius - 1.0f, 0.0f };
	vi2 t     = { 1, 1 };
	f32 error = t.x - 2.0f * radius;

	while (p.x >= p.y)
	{
		SDL_RenderDrawPointF(renderer, center.x + p.x, center.y - p.y);
		SDL_RenderDrawPointF(renderer, center.x + p.x, center.y + p.y);
		SDL_RenderDrawPointF(renderer, center.x - p.x, center.y - p.y);
		SDL_RenderDrawPointF(renderer, center.x - p.x, center.y + p.y);
		SDL_RenderDrawPointF(renderer, center.x + p.y, center.y - p.x);
		SDL_RenderDrawPointF(renderer, center.x + p.y, center.y + p.x);
		SDL_RenderDrawPointF(renderer, center.x - p.y, center.y - p.x);
		SDL_RenderDrawPointF(renderer, center.x - p.y, center.y + p.x);

		if (error <= 0.0f)
		{
			p.y   += 1.0f;
			error += t.y;
			t.y   += 2;
		}

		if (error > 0.0f)
		{
			p.x   -= 1.0f;
			t.x   += 2;
			error += t.x - 2.0f * radius;
		}
	}
}

internal void render_filled_circle(SDL_Renderer* renderer, vf2 center, f32 radius)
{
	ASSERT(radius < 256.0f);

	vf2 p     = { radius - 1.0f, 0.0f };
	vi2 t     = { 1, 1 };
	f32 error = t.x - 2.0f * radius;

	while (p.x >= p.y)
	{
		SDL_RenderDrawLineF(renderer, center.x - p.x, center.y - p.y, center.x + p.x, center.y - p.y);
		SDL_RenderDrawLineF(renderer, center.x - p.y, center.y - p.x, center.x + p.y, center.y - p.x);
		SDL_RenderDrawLineF(renderer, center.x - p.x, center.y + p.y, center.x + p.x, center.y + p.y);
		SDL_RenderDrawLineF(renderer, center.x - p.y, center.y + p.x, center.x + p.y, center.y + p.x);

		if (error <= 0.0f)
		{
			p.y   += 1.0f;
			error += t.y;
			t.y   += 2;
		}

		if (error > 0.0f)
		{
			p.x   -= 1.0f;
			t.x   += 2;
			error += t.x - 2.0f * radius;
		}
	}
}

internal void render_filled_rect(SDL_Renderer* renderer, vf2 bottom_left, vf2 dimensions)
{
	SDL_FRect rect = { bottom_left.x, bottom_left.y, dimensions.x, dimensions.y };
	SDL_RenderFillRectF(renderer, &rect);
}

internal void render_rect(SDL_Renderer* renderer, vf2 bottom_left, vf2 dimensions)
{
	SDL_FRect rect = { bottom_left.x, bottom_left.y, dimensions.x, dimensions.y };
	SDL_RenderDrawRectF(renderer, &rect);
}

internal void render_texture(SDL_Renderer* renderer, SDL_Texture* texture, vf2 position, vf2 dimensions)
{
	SDL_FRect rect = { position.x, position.y, dimensions.x, dimensions.y };
	SDL_RenderCopyF(renderer, texture, 0, &rect);
}

template <typename... ARGUMENTS>
internal void render_text(SDL_Renderer* renderer, FC_Font* font, vf2 coordinates, f32 baseline_offset, FC_AlignEnum alignment, f32 scalar, vf4 color, strlit fstr, ARGUMENTS... arguments)
{
	FC_DrawEffect
	(
		font,
		renderer,
		coordinates.x,
		coordinates.y - FC_GetBaseline(font) * scalar * baseline_offset,
		FC_MakeEffect
		(
			alignment,
			FC_MakeScale(scalar, scalar),
			FC_MakeColor
			(
				static_cast<u8>(color.x * 255.0f),
				static_cast<u8>(color.y * 255.0f),
				static_cast<u8>(color.z * 255.0f),
				static_cast<u8>(color.w * 255.0f)
			)
		),
		fstr,
		arguments...
	);
}

template <typename... ARGUMENTS>
internal void render_text(SDL_Renderer* renderer, FC_Font* font, vf2 coordinates, f32 baseline_offset, FC_AlignEnum alignment, f32 scalar, vf3 color, strlit fstr, ARGUMENTS... arguments)
{
	render_text(renderer, font, coordinates, baseline_offset, alignment, scalar, vxx(color, 1.0f), fstr, arguments...);
}

template <typename... ARGUMENTS>
internal void render_boxed_text(SDL_Renderer* renderer, FC_Font* font, vf2 coordinates, vf2 dimensions, FC_AlignEnum alignment, f32 scalar, vf4 color, strlit fstr, ARGUMENTS... arguments)
{
	FC_DrawBoxEffect
	(
		font,
		renderer,
		{ static_cast<i32>(coordinates.x), static_cast<i32>(coordinates.y), static_cast<i32>(dimensions.x / scalar), static_cast<i32>(dimensions.y) },
		FC_MakeEffect
		(
			alignment,
			FC_MakeScale(scalar, scalar),
			FC_MakeColor
			(
				static_cast<u8>(color.x * 255.0f),
				static_cast<u8>(color.y * 255.0f),
				static_cast<u8>(color.z * 255.0f),
				static_cast<u8>(color.w * 255.0f)
			)
		),
		fstr,
		arguments...
	);
}

template <typename... ARGUMENTS>
internal void render_boxed_text(SDL_Renderer* renderer, FC_Font* font, vf2 coordinates, vf2 dimensions, FC_AlignEnum alignment, f32 scalar, vf3 color, strlit fstr, ARGUMENTS... arguments)
{
	render_boxed_text(renderer, font, coordinates, dimensions, alignment, scalar, vx4(color, 1.0f), fstr, arguments)
}

internal void render_line(SDL_Renderer* renderer, vf2 start, vf2 end)
{
	ASSERT(fabsf(start.x - end.x) + fabsf(start.y - end.y) < 4096.0f);
	SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
}
