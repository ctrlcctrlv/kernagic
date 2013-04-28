                                                                            /*
Kernagic a libre auto spacer and kerner for Unified Font Objects.
Copyright (C) 2013 Øyvind Kolås

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.       */

#include <string.h>
#include <math.h>
#include <assert.h>
#include "kernagic.h"
#include "kerner.h"

extern float scale_factor;

KernerSettings kerner_settings;

static float graylevel = 0.23;
static gboolean visual_debug_enabled = FALSE;

#define DIST_MAX  120

static int      s_width  = 0;
static int      s_height = 0;
static uint8_t *scratch  = NULL;
static uint8_t *scratch2 = NULL;
static uint8_t *scratch3 = NULL;
static long int scratch3_unicode  = -1;

static void place_a (Glyph *left, Glyph *right, float opacity);

static void save_scratch (void)
{
  memcpy (scratch2, scratch, s_width * s_height);
}


static int  compute_dist (Glyph *left, Glyph *right, int space)
{
  int x, y;
  int min_dist = 2000;

  if (scratch3_unicode == left->unicode)
    {
      memcpy (scratch2, scratch3, s_width * s_height);
      memcpy (scratch, scratch3, s_width * s_height);
    }
  else
    {
      memset (scratch, 0, s_height * s_width);
      place_a (left, right, 1.0);

      for (y = 1; y < s_height-1; y++)
        for (x = 1; x < s_width-1; x++)
            if (scratch [y * s_width + x] < 254)
              scratch [y * s_width + x] = 0;

      for (int j = 0; j < DIST_MAX; j++)
        {
          save_scratch ();
          for (y = 1; y < s_height-1; y++)
            for (x = 1; x < s_width-1; x++)
              {
                if (scratch2 [y * s_width + x] < 254)
                  {
                    if (scratch2 [y * s_width + x -1] ||
                        scratch2 [y * s_width + x +1] ||
                        scratch2 [y * s_width + x - s_width] ||
                        scratch2 [y * s_width + x + s_width])
                     scratch [y * s_width + x] ++;
                  }
              }
        }
      save_scratch ();

      scratch3_unicode = left->unicode;
      memcpy (scratch3, scratch2, s_width * s_height);
    }


  for (y = 0; y < right->r_height; y++)
    for (x = 0; x < right->r_width; x++)
      if (
          x + space < s_width &&
          x + space > 0 &&
          y < s_height &&
          
          right->raster[y * right->r_width + x] > 170)
        {
          if (scratch2 [y * s_width + x + space] > 0 &&
              DIST_MAX-scratch2 [y * s_width + x + space] < min_dist)
            min_dist = DIST_MAX-scratch [y * s_width + x + space];
          scratch [y * s_width + x + space] = 200;
        }

  if (min_dist < 0)
    min_dist = -1;
  if (min_dist >= 1000)
    min_dist = -1;
  return min_dist;
}

static void update_stats (Glyph *left, Glyph *right, int s)
{
  int x0, y0, x1, y1;
  int x, y;
  int count = 0;

  graylevel = 0;
  count = 0;
  y0 = kernagic_x_height () * 1.0 * scale_factor;
  y1 = kernagic_x_height () * 2.0 * scale_factor;
  x0 = left->r_width * scale_factor / 2;
  x1 = s + right->r_width * scale_factor / 2;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      {
        graylevel += scratch [y * s_width + x];
        count ++;
  //      scratch [y * s_width + x] += 120;
      }
  graylevel = graylevel / count / 127.0;

}

static void place_a (Glyph *left, Glyph *right, float opacity)
{
  int x, y;

  for (y = 0; y < left->r_height; y++)
    for (x = 0; x < left->r_width; x++)
      if (x < s_width && y < s_height)
      scratch [y * s_width + x] = left->raster[y * left->r_width + x] * opacity;
}

static void place_glyphs (Glyph *left,
                          Glyph *right,
                          float        spacing)
{
  int x, y;
  int space = spacing;
  assert (left);
  assert (right);

  memset (scratch, 0, s_height * s_width);
  place_a (left, right, 0.5);

  for (y = 0; y < right->r_height; y++)
    for (x = 0; x < right->r_width; x++)
      if (x + space < s_width &&
          x + space >= 0 && 
          y < s_height)
        scratch [y * s_width + x + space] += right->raster[y * right->r_width + x] / 2;
}

static GtkWidget *drawing_area;

float kerner_kern (KernerSettings *settings,
                   Glyph          *left,
                   Glyph          *right)
{
  int s;
  int min_dist;

  gint    best_advance = 0;
  gfloat  best_gray_diff = 1.0;

  int maxs = left->width * scale_factor * 1.5;

  if (maxs < kernagic_x_height () * scale_factor)
    maxs = kernagic_x_height () * scale_factor;

  for (s = left->width * scale_factor * 0.5; s < maxs; s++)
  {
    min_dist = compute_dist (left, right, s);

    if (min_dist < settings->maximum_distance * kernagic_x_height () * scale_factor &&
        
        min_dist > settings->minimum_distance * kernagic_x_height () * scale_factor)
      {
        place_glyphs (left, right, s);
        update_stats (left, right, s);

        float graydiff = fabs (graylevel - settings->gray_target / 100.0);
        if (graydiff < best_gray_diff)
          {
            best_gray_diff = graydiff;
            best_advance = s;
          }

        if (visual_debug_enabled)
          {
            gtk_widget_queue_draw (drawing_area);
            int i;
            for (i = 0; i < 1500; i++)
              {
                gtk_main_iteration_do (FALSE);
              }
          }

      }


  }
  return best_advance / scale_factor;
}


static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  cairo_set_source_rgb (cr, 1,1,1);
  cairo_paint (cr);

  cairo_surface_t *g_surface;
  
  g_surface = cairo_image_surface_create_for_data (scratch, CAIRO_FORMAT_A8,
      s_width, s_height, s_width);

  cairo_set_source_rgb (cr, 1,0,0);
  cairo_translate (cr, 0, 0);
  cairo_mask_surface (cr, g_surface, 0, 0);
  cairo_fill (cr);

  cairo_surface_destroy (g_surface);

  cairo_set_source_rgb (cr, 0,0,0);
  
  cairo_select_font_face(cr, "Sans",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 20);

  char buf[4096];
  float y = 0.0;
  float x = 300;

  sprintf (buf, "graylevel: %2.2f%%", 100 * graylevel);
  cairo_move_to (cr, x, y+=30);
  cairo_show_text (cr, buf);

#if 0
  sprintf (buf, "min-dist:    %2.2f", min_dist / scale_factor);
  cairo_move_to (cr, x, y+=30);
  cairo_show_text (cr, buf);
#endif

  return FALSE;
}



void kerner_debug_ui (void)
{
  GtkWidget *window;
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  drawing_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (drawing_area, 512, 256);
  g_signal_connect (drawing_area, "draw", G_CALLBACK (draw_cb), NULL);
  gtk_container_add (GTK_CONTAINER (window), drawing_area);
  gtk_widget_show (drawing_area);
  gtk_widget_show (window);
  visual_debug_enabled = TRUE;
}

void init_kerner (void)
{
  if (scratch != NULL)
    return;

  s_width  = kernagic_x_height () * scale_factor * 4;
  s_height = kernagic_x_height () * scale_factor * 3;

  s_width /= 8;
  s_width *= 8;

  scratch  = g_malloc0 (s_width * s_height);
  scratch2 = g_malloc0 (s_width * s_height);
  scratch3 = g_malloc0 (s_width * s_height);
}
