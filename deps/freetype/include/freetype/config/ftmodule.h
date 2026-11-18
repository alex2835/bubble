/*
 * This file registers the FreeType modules compiled into the library.
 *
 * Bubble: TrueType and OpenType (CFF) outlines, their hinting, the smooth
 * renderer and OT-SVG glyphs - what fonts of games are. The other drivers
 * and renderers are left out of the build (deps/CMakeLists.txt).
 *
 */

FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_svg_renderer_class )

/* EOF */
