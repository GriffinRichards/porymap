*************
Project Files
*************

Porymap relies on the user maintaining a certain level of integrity with their project files.
This is a list of files that porymap reads from and writes to. Generally, if porymap writes 
to a file, it probably is not a good idea to edit yourself unless otherwise noted.

The filepath that Porymap expects for each file can be overridden on the ``Files`` tab of ``Options -> Project Settings``. A new path can be specified by entering it in the text box or choosing it with the |button-folder| button. Paths are expected to be relative to the root project folder. If no path is specified, or if the file/folder specified does not exist, then the default path will be used instead. The name of each setting in this section is listed in the table below under ``Override``.

.. |button-folder| image:: images/scripting-capabilities/folder.png
   :width: 24
   :height: 24

.. figure:: images/settings-and-options/tab-files.png
   :alt: Files tab

.. _files:

.. csv-table::
   :header: File Name,Read,Write,Override,Notes
   :widths: 20, 5, 5, 20, 30

   data/maps/\*/map.json, yes, yes, ``data_map_folders``,
   data/maps/\*/scripts.[inc|pory], yes, no, ``data_map_folders``, for finding script labels
   data/scripts/\*.[inc|pory], yes, no, ``data_scripts_folders``, for finding script labels
   data/event_scripts.s, no, yes, ``data_event_scripts``, only appends new script files to end of file
   data/maps/map_groups.json, yes, yes, ``json_map_groups``,
   data/layouts/layouts.json, yes, yes, ``json_layouts``,
   data/layouts/\*/[border|map].bin, yes, yes, ``data_layouts_folders``,
   src/data/tilesets/headers.h, yes, yes, ``tilesets_headers``,
   src/data/tilesets/graphics.h, yes, yes, ``tilesets_graphics``, also edits palette and tile image files listed in this file
   src/data/tilesets/metatiles.h, yes, yes, ``tilesets_metatiles``, also edits metatile files listed in this file
   data/tilesets/headers.inc, yes, yes, ``tilesets_headers_asm``, only if ``tilesets_headers`` can't be found
   data/tilesets/graphics.inc, yes, yes, ``tilesets_graphics_asm``, only if ``tilesets_headers`` can't be found
   data/tilesets/metatiles.inc, yes, yes, ``tilesets_metatiles_asm``, only if ``tilesets_headers`` can't be found
   data/tilesets/[primary|secondary]/\*, yes, yes, ``data_tilesets_folders``, default tileset data location
   src/data/wild_encounters.json, yes, yes, ``json_wild_encounters``, optional (only required to use Wild Pokémon tab)
   src/data/heal_locations.json, yes, yes, ``json_heal_locations``,
   src/data/object_events/object_event_graphics_info_pointers.h, yes, no, ``data_obj_event_gfx_pointers``,
   src/data/object_events/object_event_graphics_info.h, yes, no, ``data_obj_event_gfx_info``,
   src/data/object_events/object_event_pic_tables.h, yes, no, ``data_obj_event_pic_tables``,
   src/data/object_events/object_event_graphics.h, yes, no, ``data_obj_event_gfx``,
   src/data/graphics/pokemon.h, yes, no, ``data_pokemon_gfx``, for pokemon sprite icons
   src/data/region_map/region_map_sections.json, yes, yes, ``json_region_map_entries``,
   src/data/region_map/porymap_config.json, yes, yes, ``json_region_porymap_cfg``,
   include/constants/global.h, yes, no, ``constants_global``, reads ``define_obj_event_count``
   include/constants/items.h, yes, no, ``constants_items``, for Hidden Item events
   include/constants/flags.h, yes, no, ``constants_flags``, for Object and Hidden Item events
   include/constants/vars.h, yes, no, ``constants_vars``, for Trigger events
   include/constants/weather.h, yes, no, ``constants_weather``, for map weather and Weather Triggers
   include/constants/songs.h, yes, no, ``constants_songs``, for map music
   include/constants/pokemon.h, yes, no, ``constants_pokemon``, reads ``define_min_level`` and ``define_max_level``
   include/constants/map_types.h, yes, no, ``constants_map_types``,
   include/constants/trainer_types.h, yes, no, ``constants_trainer_types``, for Object events
   include/constants/secret_bases.h, yes, no, ``constants_secret_bases``, pokeemerald and pokeruby only
   include/constants/event_object_movement.h, yes, no, ``constants_obj_event_movement``,
   include/constants/event_objects.h, yes, no, ``constants_obj_events``,
   include/constants/event_bg.h, yes, no, ``constants_event_bg``,
   include/constants/metatile_labels.h, yes, yes, ``constants_metatile_labels``,
   include/constants/metatile_behaviors.h, yes, no, ``constants_metatile_behaviors``,
   include/constants/species.h, yes, no, ``constants_metatile_behaviors``, for the Wild Pokémon tab
   include/global.fieldmap.h, yes, no, ``global_fieldmap``, reads map and tileset data masks
   include/fieldmap.h, yes, no, ``constants_fieldmap``, reads tileset related constants
   src/fieldmap.c, yes, no, ``fieldmap``, reads ``symbol_attribute_table``
   src/event_object_movement.c, yes, no, ``initial_facing_table``, reads ``symbol_facing_directions``
   src/wild_encounter.c, yes, no, ``wild_encounter``, reads ``define_max_encounter_rate``
   src/pokemon_icon.c, yes, no, ``pokemon_icon_table``, reads files in ``symbol_pokemon_icon_table``
   graphics/pokemon/\*/icon.png, yes, no, ``pokemon_gfx``, to search for Pokémon icons if they aren't found in ``symbol_pokemon_icon_table``


In addition to these files, there are some specific symbol and macro names that Porymap expects to find in your project. These can be overridden on the ``Identifiers`` tab of ``Options -> Project Settings``. The name of each setting in this section is listed in the table below under ``Override``. Overrides with ``regex`` in the name support the `regular expression syntax <https://perldoc.perl.org/perlre>`_ used by Qt.

.. figure:: images/settings-and-options/tab-identifiers.png
   :alt: Files tab

.. _identifiers:

.. csv-table::
   :header: Override,Default,Notes
   :widths: 20, 20, 30

   ``symbol_facing_directions``, ``gInitialMovementTypeFacingDirections``, to set sprite frame for Object Events based on movement type
   ``symbol_obj_event_gfx_pointers``, ``gObjectEventGraphicsInfoPointers``, to map Object Event graphics IDs to graphics data
   ``symbol_pokemon_icon_table``, ``gMonIconTable``, to map species constants to icon images
   ``symbol_attribute_table``, ``sMetatileAttrMasks``, optionally read to get settings on ``Tilesets`` tab
   ``symbol_tilesets_prefix``, ``gTileset_``, for new tileset names and to extract base tileset names
   ``symbol_dynamic_map_name``, ``Dynamic``, reserved map name to display for ``define_map_dynamic``
   ``define_obj_event_count``, ``OBJECT_EVENT_TEMPLATES_COUNT``, to limit total Object Events
   ``define_min_level``, ``MIN_LEVEL``, minimum wild encounters level
   ``define_max_level``, ``MAX_LEVEL``, maximum wild encounters level
   ``define_max_encounter_rate``, ``MAX_ENCOUNTER_RATE``, this value / 16 will be the maximum encounter rate on the ``Wild Pokémon`` tab
   ``define_tiles_primary``, ``NUM_TILES_IN_PRIMARY``, 
   ``define_tiles_total``, ``NUM_TILES_TOTAL``,
   ``define_metatiles_primary``, ``NUM_METATILES_IN_PRIMARY``, total metatiles are calculated using metatile ID mask
   ``define_pals_primary``, ``NUM_PALS_IN_PRIMARY``,
   ``define_pals_total``, ``NUM_PALS_TOTAL``, 
   ``define_tiles_per_metatile``, ``NUM_TILES_PER_METATILE``, to determine if triple-layer metatiles are in use. Values other than 8 or 12 are ignored
   ``define_map_size``, ``MAX_MAP_DATA_SIZE``, to limit map dimensions
   ``define_mask_metatile``, ``MAPGRID_METATILE_ID_MASK``, optionally read to get settings on ``Maps`` tab
   ``define_mask_collision``, ``MAPGRID_COLLISION_MASK``, optionally read to get settings on ``Maps`` tab
   ``define_mask_elevation``, ``MAPGRID_ELEVATION_MASK``, optionally read to get settings on ``Maps`` tab
   ``define_mask_behavior``, ``METATILE_ATTR_BEHAVIOR_MASK``, optionally read to get settings on ``Tilesets`` tab
   ``define_mask_layer``, ``METATILE_ATTR_LAYER_MASK``, optionally read to get settings on ``Tilesets`` tab
   ``define_attribute_behavior``, ``METATILE_ATTRIBUTE_BEHAVIOR``, name used to extract setting from ``symbol_attribute_table``
   ``define_attribute_layer``, ``METATILE_ATTRIBUTE_LAYER_TYPE``, name used to extract setting from ``symbol_attribute_table``
   ``define_attribute_terrain``, ``METATILE_ATTRIBUTE_TERRAIN``, name used to extract setting from ``symbol_attribute_table``
   ``define_attribute_encounter``, ``METATILE_ATTRIBUTE_ENCOUNTER_TYPE``, name used to extract setting from ``symbol_attribute_table``
   ``define_metatile_label_prefix``, ``METATILE_``, expected prefix for metatile label macro names
   ``define_heal_locations_prefix``, ``HEAL_LOCATION_``, default prefix for heal location macro names
   ``define_layout_prefix``, ``LAYOUT_``, default prefix for layout macro names
   ``define_map_prefix``, ``MAP_``, expected prefix for map macro names
   ``define_map_dynamic``, ``DYNAMIC``, macro name after prefix for Dynamic maps
   ``define_map_empty``, ``UNDEFINED``, macro name after prefix for empty maps
   ``define_map_section_prefix``, ``MAPSEC_``, expected prefix for location macro names
   ``define_map_section_empty``, ``NONE``, macro name after prefix for empty region map sections
   ``define_species_prefix``, ``SPECIES_``, expected prefix for species macro names
   ``define_species_empty``, ``NONE``, macro name after prefix for the default species
   ``regex_behaviors``, ``\bMB_``, regex to find metatile behavior macro names
   ``regex_obj_event_gfx``, ``\bOBJ_EVENT_GFX_``, regex to find Object Event graphics ID macro names
   ``regex_items``, ``\bITEM_(?!(B_)?USE_)``, regex to find item macro names
   ``regex_flags``, ``\bFLAG_``, regex to find flag macro names
   ``regex_vars``, ``\bVAR_``, regex to find var macro names
   ``regex_movement_types``, ``\bMOVEMENT_TYPE_``, regex to find movement type macro names
   ``regex_map_types``, ``\bMAP_TYPE_``, regex to find map type macro names
   ``regex_battle_scenes``, ``\bMAP_BATTLE_SCENE_``, regex to find battle scene macro names
   ``regex_weather``, ``\bWEATHER_``, regex to find map weather macro names
   ``regex_coord_event_weather``, ``\bCOORD_EVENT_WEATHER_``, regex to find weather trigger macro names
   ``regex_secret_bases``, ``\bSECRET_BASE_[A-Za-z0-9_]*_[0-9]+``, regex to find secret base ID macro names
   ``regex_sign_facing_directions``, ``\bBG_EVENT_PLAYER_FACING_``, regex to find sign facing direction macro names
   ``regex_trainer_types``, ``\bTRAINER_TYPE_``, regex to find trainer type macro names
   ``regex_music``, ``\b(SE|MUS)_``, regex to find music macro names
   ``regex_gbapal``, ``\.gbapal(\.[\w]+)?$``, regex to get the expected file extension for ``.pal`` data files
   ``regex_bpp``, ``\.[\d]+bpp(\.[\w]+)?$``, regex to get the expected file extension for ``.png`` data files
   ``pals_output_extension``, ``.gbapal``, the file extension to output for a new tileset's palette data files
   ``tiles_output_extension``, ``.4bpp.lz``, the file extension to output for a new tileset's tiles image data file
