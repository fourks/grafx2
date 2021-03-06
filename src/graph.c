/* vim:expandtab:ts=2 sw=2:
*/
/*  Grafx2 - The Ultimate 256-color bitmap paint program

    Copyright 2008 Franck Charlet
    Copyright 2007 Adrien Destugues
    Copyright 1996-2001 Sunset Design (Guillaume Dorme & Karl Maritaud)

    Grafx2 is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; version 2
    of the License.

    Grafx2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grafx2; if not, see <http://www.gnu.org/licenses/>

********************************************************************************

    Drawing functions and effects.

*/

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "global.h"
#include "struct.h"
#include "engine.h"
#include "buttons.h"
#include "pages.h"
#include "errors.h"
#include "sdlscreen.h"
#include "graph.h"
#include "misc.h"
#include "pxsimple.h"
#include "windows.h"
#include "input.h"
#include "brush.h"
#include "tiles.h"

#if defined(__VBCC__) || defined(__GP2X__) || defined(__WIZ__) || defined(__CAANOO__)
    #define M_PI 3.141592653589793238462643
#endif

// Generic pixel-drawing function.
Func_pixel Pixel_figure;

// Fonction qui met � jour la zone de l'image donn�e en param�tre sur l'�cran.
// Tient compte du d�calage X et Y et du zoom, et fait tous les controles n�cessaires
void Update_part_of_screen(short x, short y, short width, short height)
{
  short effective_w, effective_h;
  short effective_X;
  short effective_Y;
  short diff;

  // Premi�re �tape, si L ou H est n�gatif, on doit remettre la zone � l'endroit
  if (width < 0)
  {
    x += width;
    width = - width;
  }

  if (height < 0)
  {
    y += height;
    height = - height;
  }

  // D'abord on met � jour dans la zone �cran normale
  diff = x-Main_offset_X;
  if (diff<0)
  {
    effective_w = width + diff;
    effective_X = 0;
  }
  else
  {
    effective_w = width;
    effective_X = diff;
  }
  diff = y-Main_offset_Y;
  if (diff<0)
  {
    effective_h = height + diff;
    effective_Y = 0;
  }
  else
  {
    effective_h = height;
    effective_Y = diff;
  }

  // Normalement il ne faudrait pas updater au del� du split quand on est en mode loupe,
  // mais personne ne devrait demander d'update en dehors de cette limite, m�me le fill est contraint
  // a rester dans la zone visible de l'image
  // ...Sauf l'affichage de brosse en preview - yr
  if(Main_magnifier_mode && effective_X + effective_w > Main_separator_position)
    effective_w = Main_separator_position - effective_X;
  else if(effective_X + effective_w > Screen_width)
    effective_w = Screen_width - effective_X;

  if(effective_Y + effective_h > Menu_Y)
    effective_h = Menu_Y - effective_Y;

  /*
  SDL_Rect r;
  r.x=effective_X;
  r.y=effective_Y;
  r.h=effective_h;
  r.w=effective_w;
  SDL_FillRect(Screen_SDL,&r,3);
  */
  Update_rect(effective_X,effective_Y,effective_w,effective_h);

  // Et ensuite dans la partie zoom�e
  if(Main_magnifier_mode)
  {
    // Clipping en X
    effective_X = (x-Main_magnifier_offset_X)*Main_magnifier_factor;
    effective_Y = (y-Main_magnifier_offset_Y)*Main_magnifier_factor;
    effective_w = width * Main_magnifier_factor;
    effective_h = height * Main_magnifier_factor;

    if (effective_X < 0)
    {
      effective_w+=effective_X;
      if (effective_w<0)
        return;

      effective_X = Main_separator_position + SEPARATOR_WIDTH*Menu_factor_X;
    }
    else
      effective_X += Main_separator_position + SEPARATOR_WIDTH*Menu_factor_X;
    diff = effective_X+effective_w-Min(Screen_width, Main_X_zoom+(Main_image_width-Main_magnifier_offset_X)*Main_magnifier_factor);
    if (diff>0)
    {
      effective_w -=diff;
      if (effective_w<0)
        return;
    }


    // Clipping en Y
    if (effective_Y < 0)
    {
      effective_h+=effective_Y;
      if (effective_h<0)
        return;
      effective_Y = 0;
    }
    diff = effective_Y+effective_h-Min(Menu_Y, (Main_image_height-Main_magnifier_offset_Y)*Main_magnifier_factor);
    if (diff>0)
    {
      effective_h -=diff;
      if (effective_h<0)
        return;
    }

 // Tr�s utile pour le debug :)
    /*SDL_Rect r;
    r.x=effective_X;
    r.y=effective_Y;
    r.h=effective_h;
    r.w=effective_w;
    SDL_FillRect(Screen_SDL,&r,3);*/
  }
}



void Transform_point(short x, short y, float cos_a, float sin_a,
                       short * rx, short * ry)
{
  *rx=Round(((float)x*cos_a)+((float)y*sin_a));
  *ry=Round(((float)y*cos_a)-((float)x*sin_a));
}


//--------------------- Initialisation d'un mode vid�o -----------------------

int Init_mode_video(int width, int height, int fullscreen, int pix_ratio)
{
  int index;
  int factor;
  int pix_width;
  int pix_height;
  byte screen_changed;
  byte pixels_changed;
  int absolute_mouse_x=Mouse_X;
  int absolute_mouse_y=Mouse_Y;
  static int Wrong_resize;

try_again:

  switch (pix_ratio)
  {
      default:
      case PIXEL_SIMPLE:
          pix_width=1;
          pix_height=1;
      break;
      case PIXEL_TALL:
          pix_width=1;
          pix_height=2;
      break;
      case PIXEL_WIDE:
          pix_width=2;
          pix_height=1;
      break;
      case PIXEL_DOUBLE:
          pix_width=2;
          pix_height=2;
      break;
      case PIXEL_TRIPLE:
          pix_width=3;
          pix_height=3;
      break;
      case PIXEL_WIDE2:
          pix_width=4;
          pix_height=2;
      break;
      case PIXEL_TALL2:
          pix_width=2;
          pix_height=4;
      break;
      case PIXEL_QUAD:
          pix_width=4;
          pix_height=4;
      break;
  }

  screen_changed = (Screen_width!=width ||
                    Screen_height!=height ||
                    Video_mode[Current_resolution].Fullscreen != fullscreen);

  // Valeurs raisonnables: minimum 320x200
  if (!fullscreen)
  {
    if (width > 320*Menu_factor_X && height > 200*Menu_factor_Y)
      Wrong_resize = 0;

    if (width < 320*Menu_factor_X)
    {
        width = 320*Menu_factor_X;
        screen_changed=1;
        Wrong_resize++;
    }
    if (height < 200*Menu_factor_Y)
    {
        height = 200*Menu_factor_Y;
        screen_changed=1;
        Wrong_resize++;
    }
    Video_mode[0].Width = width;
    Video_mode[0].Height = height;

  }
  else
  {
    if (width < 320*Menu_factor_X || height < 200*Menu_factor_Y)
      return 1;
  }
  // La largeur doit �tre un multiple de 4
  #ifdef __amigaos4__
      // On AmigaOS the systems adds some more constraints on that ...
      width = (width + 15) & 0xFFFFFFF0;
  #else
      //width = (width + 3 ) & 0xFFFFFFFC;
  #endif

  pixels_changed = (Pixel_ratio!=pix_ratio);

  if (!screen_changed && !pixels_changed)
    return 0;
  if (screen_changed)
  {
    Set_mode_SDL(&width, &height,fullscreen);
  }

  if (screen_changed || pixels_changed)
  {
    Pixel_ratio=pix_ratio;
    Pixel_width=pix_width;
    Pixel_height=pix_height;
  }
  Screen_width = width;
  Screen_height = height;

  /*
  // Set menu size (software zoom)
  if (Screen_width/320 > Screen_height/200)
    factor=Screen_height/200;
  else
    factor=Screen_width/320;

  switch (Config.Ratio)
  {
    case 1: // Always the biggest possible
      Menu_factor_X=factor;
      Menu_factor_Y=factor;
      break;
    case 2: // Only keep the aspect ratio
      Menu_factor_X=factor-1;
      if (Menu_factor_X<1) Menu_factor_X=1;
      Menu_factor_Y=factor-1;
      if (Menu_factor_Y<1) Menu_factor_Y=1;
      break;
    case 0: // Always smallest possible
      Menu_factor_X=1;
      Menu_factor_Y=1;
      break;
    default: // Stay below some reasonable size
      Menu_factor_X=Min(factor,abs(Config.Ratio));
      Menu_factor_Y=Min(factor,abs(Config.Ratio));
  }
  */

  // Clear previous menu textures
  //for (index=0; index<MENUBAR_COUNT; index++)
  //{
  //  SDL_DestroyTexture(Menu_bars[index].Menu_texture);
  //  Menu_bars[index].Menu_texture = NULL;
  //}

  Set_palette(Main_palette);

  Current_resolution=0;
  if (fullscreen)
  {
    for (index=1; index<Nb_video_modes; index++)
    {
      if (Video_mode[index].Width==Screen_width &&
          Video_mode[index].Height==Screen_height)
      {
        Current_resolution=index;
        break;
      }
    }
  }

  Change_palette_cells();

  Menu_Y = Screen_height;
  if (Menu_is_visible)
    Menu_Y -= Menu_height * Menu_factor_Y;
  Menu_status_Y = Screen_height-(Menu_factor_Y<<3);

  Adjust_mouse_sensitivity(fullscreen);

  Mouse_X=absolute_mouse_x;
  if (Mouse_X>=Screen_width)
    Mouse_X=Screen_width-1;
  Mouse_Y=absolute_mouse_y;
  if (Mouse_Y>=Screen_height)
    Mouse_Y=Screen_height-1;
  if (fullscreen)
    Set_mouse_position();

  Spare_offset_X=0; // |  Il faut penser � �viter les incoh�rences
  Spare_offset_Y=0; // |- de d�calage du brouillon par rapport �
  Spare_magnifier_mode=0; // |  la r�solution.

  if (Main_magnifier_mode)
  {
    Pixel_preview=Pixel_preview_magnifier;
  }
  else
  {
    Pixel_preview=Pixel_preview_normal;
    // Recaler la vue (meme clipping que dans Scroll_screen())
    if (Main_offset_X+Screen_width>Main_image_width)
      Main_offset_X=Main_image_width-Screen_width;
    if (Main_offset_X<0)
      Main_offset_X=0;
    if (Main_offset_Y+Menu_Y>Main_image_height)
      Main_offset_Y=Main_image_height-Menu_Y;
    if (Main_offset_Y<0)
      Main_offset_Y=0;
  }

  Compute_magnifier_data();
  if (Main_magnifier_mode)
    Position_screen_according_to_zoom();
  Compute_limits();
  Compute_paintbrush_coordinates();

  return 0;
}



  // -- Redimentionner l'image (nettoie l'�cran virtuel) --

void Resize_image(word chosen_width,word chosen_height)
{
  word old_width=Main_image_width;
  word old_height=Main_image_height;
  int i;

  // +-+-+
  // |C| |  A+B+C = Ancienne image
  // +-+A|
  // |B| |    C   = Nouvelle image
  // +-+-+

  Upload_infos_page_main(Main_backups->Pages);
  if (Backup_with_new_dimensions(chosen_width,chosen_height))
  {
    // La nouvelle page a pu �tre allou�e, elle est pour l'instant pleine de
    // 0s. Elle fait Main_image_width de large.

    Main_image_is_modified=1;

    // On copie donc maintenant la partie C dans la nouvelle image.
    for (i=0; i<Main_backups->Pages->Nb_layers; i++)
    {
      Copy_part_of_image_to_another(
        Main_backups->Pages->Next->Image[i].Pixels,0,0,Min(old_width,Main_image_width),
        Min(old_height,Main_image_height),old_width,
        Main_backups->Pages->Image[i].Pixels,0,0,Main_image_width);
    }
    Redraw_layered_image();
  }
  else
  {
    // Afficher un message d'erreur
    Display_cursor();
    Message_out_of_memory();
    Hide_cursor();
  }
}



void Remap_spare(void)
{
  short x_pos; // Variable de balayage de la brosse
  short y_pos; // Variable de balayage de la brosse
  byte  used[256]; // Tableau de bool�ens "La couleur est utilis�e"
  int   color;
  int   layer;

  // On commence par initialiser le tableau de bool�ens � faux
  for (color=0;color<=255;color++)
    used[color]=0;

  // On calcule la table d'utilisation des couleurs
  for (layer=0; layer<Spare_backups->Pages->Nb_layers; layer++)
    for (y_pos=0;y_pos<Spare_image_height;y_pos++)
      for (x_pos=0;x_pos<Spare_image_width;x_pos++)
        used[*(Spare_backups->Pages->Image[layer].Pixels+(y_pos*Spare_image_width+x_pos))]=1;

  //   On va maintenant se servir de la table "used" comme table de
  // conversion: pour chaque indice, la table donne une couleur de
  // remplacement.
  // Note : Seules les couleurs utilis�es on besoin d'�tres recalcul�es: les
  //       autres ne seront jamais consult�es dans la nouvelle table de
  //       conversion puisque elles n'existent pas dans l'image, donc elles
  //       ne seront pas utilis�es par Remap_general_lowlevel.
  for (color=0;color<=255;color++)
    if (used[color])
      used[color]=Best_color_perceptual(Spare_palette[color].R,Spare_palette[color].G,Spare_palette[color].B);

  //   Maintenant qu'on a une super table de conversion qui n'a que le nom
  // qui craint un peu, on peut faire l'�change dans la brosse de toutes les
  // teintes.
  for (layer=0; layer<Spare_backups->Pages->Nb_layers; layer++)
    Remap_general_lowlevel(used,Spare_backups->Pages->Image[layer].Pixels,Spare_backups->Pages->Image[layer].Pixels,Spare_image_width,Spare_image_height,Spare_image_width);

  // Change transparent color index
  Spare_backups->Pages->Transparent_color=used[Spare_backups->Pages->Transparent_color];
}



void Get_colors_from_brush(void)
{
  short x_pos; // Variable de balayage de la brosse
  short y_pos; // Variable de balayage de la brosse
  byte  brush_used[256]; // Tableau de bool�ens "La couleur est utilis�e"
  dword usage[256];
  int   color;
  int   image_color;

  //if (Confirmation_box("Modify current palette ?"))

  // Backup with unchanged layers, only palette is modified
  Backup_layers(LAYER_NONE);

  // Init array of new colors
  for (color=0;color<=255;color++)
    brush_used[color]=0;

  // Tag used colors
  for (y_pos=0;y_pos<Brush_height;y_pos++)
    for (x_pos=0;x_pos<Brush_width;x_pos++)
      brush_used[*(Brush_original_pixels + y_pos * Brush_width + x_pos)]=1;

  // Check used colors in picture (to know which palette entries are free)
  Count_used_colors(usage);

  // First pass : omit colors that are already in palette
  for (color=0; color<256; color++)
  {
    // For each color used in brush (to add in palette)
    if (brush_used[color])
    {
      // Try locate it in current palette
      for (image_color=0; image_color<256; image_color++)
      {
        if (Brush_original_palette[color].R==Main_palette[image_color].R
         && Brush_original_palette[color].G==Main_palette[image_color].G
         && Brush_original_palette[color].B==Main_palette[image_color].B)
        {
          // Color already in main palette:

          // Tag as used, so that no new color will overwrite it
          usage[image_color]=1;

          // Tag as non-new, to avoid it in pass 2
          brush_used[color]=0;

          break;
        }
      }
    }
  }

  // Second pass : For each color to add, find an empty slot in
  // main palette to add it
  image_color=0;
  for (color=0; color<256 && image_color<256; color++)
  {
    // For each color used in brush
    if (brush_used[color])
    {
      for (; image_color<256; image_color++)
      {
        if (!usage[image_color])
        {
          // Copy from color to image_color
          Main_palette[image_color].R=Brush_original_palette[color].R;
          Main_palette[image_color].G=Brush_original_palette[color].G;
          Main_palette[image_color].B=Brush_original_palette[color].B;

          image_color++;
          break;
        }
      }
    }
  }
  Remap_brush();

  Set_palette(Main_palette);
  Compute_optimal_menu_colors(Main_palette);
  Hide_cursor();
  Display_all_screen();
  Display_menu();
  Display_cursor();
  End_of_modification();

  Main_image_is_modified=1;
}



//////////////////////////////////////////////////////////////////////////////
////////////////////////////// GESTION DU FILLER /////////////////////////////
//////////////////////////////////////////////////////////////////////////////


void Fill(short * top_reached  , short * bottom_reached,
          short * left_reached, short * right_reached)
//
//   Cette fonction fait un remplissage classique d'une zone d�limit�e de
// l'image. Les limites employ�es sont Limit_top, Limit_bottom, Limit_left
// et Limit_right. Le point de d�part du remplissage est Paintbrush_X,Paintbrush_Y
// et s'effectue en th�orie sur la couleur 1 et emploie la couleur 2 pour le
// remplissage. Ces restrictions sont d�es � l'utilisation qu'on en fait dans
// la fonction principale "Fill_general", qui se charge de faire une gestion de
// tous les effets.
//   Cette fonction ne doit pas �tre directement appel�e.
//
{
  short x_pos;   // Abscisse de balayage du segment, utilis�e lors de l'"affichage"
  short line;   // Ordonn�e de la ligne en cours de traitement
  short start_x; // Abscisse de d�part du segment trait�
  short end_x;   // Abscisse de fin du segment trait�
  int   changes_made;    // Bool�en "On a fait une modif dans le dernier passage"
  int   can_propagate; // Bool�en "On peut propager la couleur dans le segment"
  short current_limit_bottom;  // Intervalle vertical restreint
  short current_limit_top;
  int   line_is_modified;       // Bool�en "On a fait une modif dans la ligne"

  changes_made=1;
  current_limit_top=Paintbrush_Y;
  current_limit_bottom =Min(Paintbrush_Y+1,Limit_bottom);
  *left_reached=Paintbrush_X;
  *right_reached=Paintbrush_X+1;
  Pixel_in_current_layer(Paintbrush_X,Paintbrush_Y,2);

  while (changes_made)
  {
    changes_made=0;

    for (line=current_limit_top;line<=current_limit_bottom;line++)
    {
      line_is_modified=0;
      // On va traiter le cas de la ligne n� line.

      // On commence le traitement � la gauche de l'�cran
      start_x=Limit_left;

      // Pour chaque segment de couleur 1 que peut contenir la ligne
      while (start_x<=Limit_right)
      {
        // On cherche son d�but
        while((start_x<=Limit_right) &&
                (Read_pixel_from_current_layer(start_x,line)!=1))
             start_x++;

        if (start_x<=Limit_right)
        {
          // Un segment de couleur 1 existe et commence � la position start_x.
          // On va donc en chercher la fin.
          for (end_x=start_x+1;(end_x<=Limit_right) &&
               (Read_pixel_from_current_layer(end_x,line)==1);end_x++);

          //   On sait qu'il existe un segment de couleur 1 qui commence en
          // start_x et qui se termine en end_x-1.

          //   On va maintenant regarder si une couleur sur la p�riph�rie
          // permet de colorier ce segment avec la couleur 2.

          can_propagate=(
            // Test de la pr�sence d'un point � gauche du segment
            ((start_x>Limit_left) &&
             (Read_pixel_from_current_layer(start_x-1,line)==2)) ||
            // Test de la pr�sence d'un point � droite du segment
            ((end_x-1<Limit_right) &&
             (Read_pixel_from_current_layer(end_x    ,line)==2))
                               );

          // Test de la pr�sence d'un point en haut du segment
          if (!can_propagate && (line>Limit_top))
            for (x_pos=start_x;x_pos<end_x;x_pos++)
              if (Read_pixel_from_current_layer(x_pos,line-1)==2)
              {
                can_propagate=1;
                break;
              }

          if (can_propagate)
          {
            if (start_x<*left_reached)
              *left_reached=start_x;
            if (end_x>*right_reached)
              *right_reached=end_x;
            // On remplit le segment de start_x � end_x-1.
            for (x_pos=start_x;x_pos<end_x;x_pos++)
              Pixel_in_current_layer(x_pos,line,2);
            // On vient d'effectuer des modifications.
            changes_made=1;
            line_is_modified=1;
          }

          start_x=end_x+1;
        }
      }

      // Si on est en bas, et qu'on peut se propager vers le bas...
      if ( (line==current_limit_bottom) &&
           (line_is_modified) &&
           (current_limit_bottom<Limit_bottom) )
        current_limit_bottom++; // On descend cette limite vers le bas
    }

    // Pour le prochain balayage vers le haut, on va se permettre d'aller
    // voir une ligne plus haut.
    // Si on ne le fait pas, et que la premi�re ligne (current_limit_top)
    // n'�tait pas modifi�e, alors cette limite ne serait pas remont�e, donc
    // le filler ne progresserait pas vers le haut.
    if (current_limit_top>Limit_top)
      current_limit_top--;

    for (line=current_limit_bottom;line>=current_limit_top;line--)
    {
      line_is_modified=0;
      // On va traiter le cas de la ligne n� line.

      // On commence le traitement � la gauche de l'�cran
      start_x=Limit_left;

      // Pour chaque segment de couleur 1 que peut contenir la ligne
      while (start_x<=Limit_right)
      {
        // On cherche son d�but
        for (;(start_x<=Limit_right) &&
             (Read_pixel_from_current_layer(start_x,line)!=1);start_x++);

        if (start_x<=Limit_right)
        {
          // Un segment de couleur 1 existe et commence � la position start_x.
          // On va donc en chercher la fin.
          for (end_x=start_x+1;(end_x<=Limit_right) &&
               (Read_pixel_from_current_layer(end_x,line)==1);end_x++);

          //   On sait qu'il existe un segment de couleur 1 qui commence en
          // start_x et qui se termine en end_x-1.

          //   On va maintenant regarder si une couleur sur la p�riph�rie
          // permet de colorier ce segment avec la couleur 2.

          can_propagate=(
            // Test de la pr�sence d'un point � gauche du segment
            ((start_x>Limit_left) &&
             (Read_pixel_from_current_layer(start_x-1,line)==2)) ||
            // Test de la pr�sence d'un point � droite du segment
            ((end_x-1<Limit_right) &&
             (Read_pixel_from_current_layer(end_x    ,line)==2))
                               );

          // Test de la pr�sence d'un point en bas du segment
          if (!can_propagate && (line<Limit_bottom))
            for (x_pos=start_x;x_pos<end_x;x_pos++)
              if (Read_pixel_from_current_layer(x_pos,line+1)==2)
              {
                can_propagate=1;
                break;
              }

          if (can_propagate)
          {
            if (start_x<*left_reached)
              *left_reached=start_x;
            if (end_x>*right_reached)
              *right_reached=end_x;
            // On remplit le segment de start_x � end_x-1.
            for (x_pos=start_x;x_pos<end_x;x_pos++)
              Pixel_in_current_layer(x_pos,line,2);
            // On vient d'effectuer des modifications.
            changes_made=1;
            line_is_modified=1;
          }

          start_x=end_x+1;
        }
      }

      // Si on est en haut, et qu'on peut se propager vers le haut...
      if ( (line==current_limit_top) &&
           (line_is_modified) &&
           (current_limit_top>Limit_top) )
        current_limit_top--; // On monte cette limite vers le haut
    }
  }

  *top_reached=current_limit_top;
  *bottom_reached =current_limit_bottom;
  (*right_reached)--;
} // end de la routine de remplissage "Fill"

byte Read_pixel_from_backup_layer(word x,word y)
{
  return *((y)*Main_image_width+(x)+Main_backups->Pages->Next->Image[Main_current_layer].Pixels);
}

void Fill_general(byte fill_color)
//
//  Cette fonction fait un remplissage qui g�re tous les effets. Elle fait
// appel � "Fill()".
//
{
  byte   cursor_shape_before_fill;
  short  x_pos,y_pos;
  short  top_reached  ,bottom_reached;
  short  left_reached,right_reached;
  byte   replace_table[256];
  int old_limit_right=Limit_right;
  int old_limit_left=Limit_left;
  int old_limit_top=Limit_top;
  int old_limit_bottom=Limit_bottom;


  // Avant toute chose, on v�rifie que l'on n'est pas en train de remplir
  // en dehors de l'image:

  if ( (Paintbrush_X>=Limit_left) &&
       (Paintbrush_X<=Limit_right) &&
       (Paintbrush_Y>=Limit_top)   &&
       (Paintbrush_Y<=Limit_bottom) )
  {
    // If tilemap mode is ON, ignore action if it's outside grid limits
    if (Main_tilemap_mode)
    {
      if (Paintbrush_X<Snap_offset_X)
        return;
      if (Paintbrush_X >= (Main_image_width-Snap_offset_X)/Snap_width*Snap_width+Snap_offset_X)
        return;
      if (Paintbrush_Y<Snap_offset_Y)
        return;
      if (Paintbrush_Y >= (Main_image_height-Snap_offset_Y)/Snap_height*Snap_height+Snap_offset_Y)
        return;
    }

    // On suppose que le curseur est d�j� cach�.
    // Hide_cursor();

    //   On va faire patienter l'utilisateur en lui affichant un joli petit
    // sablier:
    cursor_shape_before_fill=Cursor_shape;
    Cursor_shape=CURSOR_SHAPE_HOURGLASS;
    Display_cursor();

    // On commence par effectuer un backup de l'image.
    Backup();

    // On fait attention au Feedback qui DOIT se faire avec le backup.
    Update_FX_feedback(0);

    // If tilemap mode is ON, adapt limits to current tile only
    if (Main_tilemap_mode)
    {
      Limit_right = Min(Limit_right, (Paintbrush_X-Snap_offset_X)/Snap_width*Snap_width+Snap_width-1+Snap_offset_X);
      Limit_left = Max(Limit_left, (Paintbrush_X-Snap_offset_X)/Snap_width*Snap_width+Snap_offset_X);
      Limit_bottom = Min(Limit_bottom, (Paintbrush_Y-Snap_offset_Y)/Snap_height*Snap_height+Snap_height-1+Snap_offset_Y);
      Limit_top = Max(Limit_top, (Paintbrush_Y-Snap_offset_Y)/Snap_height*Snap_height+Snap_offset_Y);
    }

    // On va maintenant "�purer" la zone visible de l'image:
    memset(replace_table,0,256);
    replace_table[Read_pixel_from_backup_layer(Paintbrush_X,Paintbrush_Y)]=1;
    Replace_colors_within_limits(replace_table);

    // On fait maintenant un remplissage classique de la couleur 1 avec la 2
    Fill(&top_reached  ,&bottom_reached,
         &left_reached,&right_reached);

    //  On s'appr�te � faire des op�rations qui n�cessitent un affichage. Il
    // faut donc retirer de l'�cran le curseur:
    Hide_cursor();
    Cursor_shape=cursor_shape_before_fill;

    //  Il va maintenant falloir qu'on "turn" ce gros caca "into" un truc qui
    // ressemble un peu plus � ce � quoi l'utilisateur peut s'attendre.
    if (top_reached>Limit_top)
      Copy_part_of_image_to_another(Main_backups->Pages->Next->Image[Main_current_layer].Pixels, // source
                                               Limit_left,Limit_top,       // Pos X et Y dans source
                                               (Limit_right-Limit_left)+1, // width copie
                                               top_reached-Limit_top,// height copie
                                               Main_image_width,         // width de la source
                                               Main_backups->Pages->Image[Main_current_layer].Pixels, // Destination
                                               Limit_left,Limit_top,       // Pos X et Y destination
                                               Main_image_width);        // width destination
    if (bottom_reached<Limit_bottom)
      Copy_part_of_image_to_another(Main_backups->Pages->Next->Image[Main_current_layer].Pixels,
                                               Limit_left,bottom_reached+1,
                                               (Limit_right-Limit_left)+1,
                                               Limit_bottom-bottom_reached,
                                               Main_image_width,Main_backups->Pages->Image[Main_current_layer].Pixels,
                                               Limit_left,bottom_reached+1,Main_image_width);
    if (left_reached>Limit_left)
      Copy_part_of_image_to_another(Main_backups->Pages->Next->Image[Main_current_layer].Pixels,
                                               Limit_left,top_reached,
                                               left_reached-Limit_left,
                                               (bottom_reached-top_reached)+1,
                                               Main_image_width,Main_backups->Pages->Image[Main_current_layer].Pixels,
                                               Limit_left,top_reached,Main_image_width);
    if (right_reached<Limit_right)
      Copy_part_of_image_to_another(Main_backups->Pages->Next->Image[Main_current_layer].Pixels,
                                               right_reached+1,top_reached,
                                               Limit_right-right_reached,
                                               (bottom_reached-top_reached)+1,
                                               Main_image_width,Main_backups->Pages->Image[Main_current_layer].Pixels,
                                               right_reached+1,top_reached,Main_image_width);

    // Restore image limits : this is needed by the tilemap effect,
    // otherwise it will not display other modified tiles.
    Limit_right=old_limit_right;
    Limit_left=old_limit_left;
    Limit_top=old_limit_top;
    Limit_bottom=old_limit_bottom;

    for (y_pos=top_reached;y_pos<=bottom_reached;y_pos++)
    {
      for (x_pos=left_reached;x_pos<=right_reached;x_pos++)
      {
        byte filled = Read_pixel_from_current_layer(x_pos,y_pos);

        // First, restore the color.
        Pixel_in_current_screen(x_pos,y_pos,Read_pixel_from_backup_layer(x_pos,y_pos));

        if (filled==2)
        {
          // Update the color according to the fill color and all effects
          Display_pixel(x_pos,y_pos,fill_color);
        }
      }
    }

    // Restore original feedback value
    Update_FX_feedback(Config.FX_Feedback);

    Update_rect(0,0,0,0);
    End_of_modification();
  }
}



//////////////////////////////////////////////////////////////////////////////
////////////////// TRAC�S DE FIGURES G�OM�TRIQUES STANDARDS //////////////////
////////////////////////// avec gestion de previews //////////////////////////
//////////////////////////////////////////////////////////////////////////////

  // Data used by ::Init_permanent_draw() and ::Pixel_figure_permanent()
  static Uint32 Permanent_draw_next_refresh=0;
  static int Permanent_draw_count=0;

  void Init_permanent_draw(void)
  {
    Permanent_draw_count = 0;
    Permanent_draw_next_refresh = SDL_GetTicks() + 100;
  }

  // Affichage d'un point de fa�on d�finitive (utilisation du pinceau)
  void Pixel_figure_permanent(word x_pos,word y_pos,byte color)
  {
    Draw_paintbrush(x_pos,y_pos,color);
    Permanent_draw_count ++;

    // Check every 8 pixels
    if (! (Permanent_draw_count&7))
    {
      Uint32 now = SDL_GetTicks();
      SDL_PumpEvents();
      if (now>= Permanent_draw_next_refresh)
      {
        Permanent_draw_next_refresh = now+100;
        Flush_update();
      }
    }
  }

  // Affichage d'un point de fa�on d�finitive
  void Pixel_clipped(word x_pos,word y_pos,byte color)
  {
    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
    Display_pixel(x_pos,y_pos,color);
  }

  // Affichage d'un point pour une preview
  void Pixel_figure_preview(word x_pos,word y_pos,byte color)
  {
    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
      Pixel_preview(x_pos,y_pos,color);
  }
  // Affichage d'un point pour une preview, avec sa propre couleur
  void Pixel_figure_preview_auto(word x_pos,word y_pos)
  {
    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
      Pixel_preview(x_pos,y_pos,Read_pixel_from_current_screen(x_pos,y_pos));
  }

  // Affichage d'un point pour une preview en xor
  void Pixel_figure_preview_xor(short x_pos,short y_pos,byte color)
  {
    (void)color; // unused

    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
      Pixel_preview(x_pos,y_pos,xor_lut[Read_pixel(x_pos-Main_offset_X,
                                           y_pos-Main_offset_Y)]);
  }

  // Affichage d'un point pour une preview en xor additif
  // (Il lit la couleur depuis la page backup)
  void Pixel_figure_preview_xorback(word x_pos,word y_pos,byte color)
  {
    (void)color; // unused

    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
      Pixel_preview(x_pos,y_pos,xor_lut[Main_screen[x_pos+y_pos*Main_image_width]]);
  }


  // Effacement d'un point de preview
  void Pixel_figure_clear_preview(word x_pos,word y_pos,byte color)
  {
    (void)color; // unused

    if ( (x_pos>=Limit_left) &&
         (x_pos<=Limit_right) &&
         (y_pos>=Limit_top)   &&
         (y_pos<=Limit_bottom) )
      Pixel_preview(x_pos,y_pos,Read_pixel_from_current_screen(x_pos,y_pos));
  }

  // Affichage d'un point dans la brosse
  void Pixel_figure_in_brush(word x_pos,word y_pos,byte color)
  {
    x_pos-=Brush_offset_X;
    y_pos-=Brush_offset_Y;
    if ( (x_pos<Brush_width) && // Les pos sont des word donc jamais < 0 ...
         (y_pos<Brush_height) )
      Pixel_in_brush(x_pos,y_pos,color);
  }


  // -- Tracer g�n�ral d'un cercle vide -------------------------------------

void Draw_empty_circle_general(short center_x,short center_y,short radius,byte color)
{
  short start_x;
  short start_y;
  short x_pos;
  short y_pos;

  // Ensuite, on va parcourire le quart haut gauche du cercle
  start_x=center_x-radius;
  start_y=center_y-radius;

  // Affichage des extremit�es du cercle sur chaque quart du cercle:
  for (y_pos=start_y,Circle_cursor_Y=-radius;y_pos<center_y;y_pos++,Circle_cursor_Y++)
    for (x_pos=start_x,Circle_cursor_X=-radius;x_pos<center_x;x_pos++,Circle_cursor_X++)
      if (Pixel_in_circle())
      {
        // On vient de tomber sur le premier point sur la ligne horizontale
        // qui fait partie du cercle.
        // Donc on peut l'afficher (lui et ses copains sym�triques)

         // Quart Haut-gauche
        Pixel_figure(x_pos,y_pos,color);
         // Quart Haut-droite
        Pixel_figure((center_x<<1)-x_pos,y_pos,color);
         // Quart Bas-droite
        Pixel_figure((center_x<<1)-x_pos,(center_y<<1)-y_pos,color);
         // Quart Bas-gauche
        Pixel_figure(x_pos,(center_y<<1)-y_pos,color);

        // On peut ensuite afficher tous les points qui le suivent dont le
        // pixel voisin du haut n'appartient pas au cercle:
        for (Circle_cursor_Y--,x_pos++,Circle_cursor_X++;x_pos<center_x;x_pos++,Circle_cursor_X++)
          if (!Pixel_in_circle())
          {
             // Quart Haut-gauche
            Pixel_figure(x_pos,y_pos,color);
             // Quart Haut-droite
            Pixel_figure((center_x<<1)-x_pos,y_pos,color);
             // Quart Bas-gauche
            Pixel_figure(x_pos,(center_y<<1)-y_pos,color);
             // Quart Bas-droite
            Pixel_figure((center_x<<1)-x_pos,(center_y<<1)-y_pos,color);
          }
          else
            break;

        Circle_cursor_Y++;
        break;
      }

  // On affiche � la fin les points cardinaux:
  Pixel_figure(center_x,center_y-radius,color); // Haut
  Pixel_figure(center_x-radius,center_y,color); // Gauche
  Pixel_figure(center_x+radius,center_y,color); // Droite
  Pixel_figure(center_x,center_y+radius,color); // Bas

}

  // -- Trac� d�finitif d'un cercle vide --

void Draw_empty_circle_permanent(short center_x,short center_y,short radius,byte color)
{
  Pixel_figure=Pixel_figure_permanent;
  Init_permanent_draw();
  Draw_empty_circle_general(center_x,center_y,radius,color);
  Update_part_of_screen(center_x - radius, center_y - radius, 2* radius+1, 2*radius+1);
}

  // -- Tracer la preview d'un cercle vide --

void Draw_empty_circle_preview(short center_x,short center_y,short radius,byte color)
{
  Pixel_figure=Pixel_figure_preview;
  Draw_empty_circle_general(center_x,center_y,radius,color);
  Update_part_of_screen(center_x - radius, center_y - radius, 2* radius+1, 2*radius+1);
}

  // -- Effacer la preview d'un cercle vide --

void Hide_empty_circle_preview(short center_x,short center_y,short radius)
{
  Pixel_figure=Pixel_figure_clear_preview;
  Draw_empty_circle_general(center_x,center_y,radius,0);
  Update_part_of_screen(center_x - radius, center_y - radius, 2* radius+1, 2*radius+1);
}

  // -- Tracer un cercle plein --

void Draw_filled_circle(short center_x,short center_y,short radius,byte color)
{
  short start_x;
  short start_y;
  short x_pos;
  short y_pos;
  short end_x;
  short end_y;

  start_x=center_x-radius;
  start_y=center_y-radius;
  end_x=center_x+radius;
  end_y=center_y+radius;

  // Correction des bornes d'apr�s les limites
  if (start_y<Limit_top)
    start_y=Limit_top;
  if (end_y>Limit_bottom)
    end_y=Limit_bottom;
  if (start_x<Limit_left)
    start_x=Limit_left;
  if (end_x>Limit_right)
    end_x=Limit_right;

  // Affichage du cercle
  for (y_pos=start_y,Circle_cursor_Y=(long)start_y-center_y;y_pos<=end_y;y_pos++,Circle_cursor_Y++)
    for (x_pos=start_x,Circle_cursor_X=(long)start_x-center_x;x_pos<=end_x;x_pos++,Circle_cursor_X++)
      if (Pixel_in_circle())
        Display_pixel(x_pos,y_pos,color);

  Update_part_of_screen(start_x,start_y,end_x+1-start_x,end_y+1-start_y);
}

int Circle_squared_diameter(int diameter)
{
  int result = diameter*diameter;
  // Trick to make some circles rounder, even though
  // mathematically incorrect.
  if (diameter==3 || diameter==9)
    return result-2;
  if (diameter==11)
    return result-6;
  if (diameter==14)
    return result-4;

  return result;
}

  // -- Tracer g�n�ral d'une ellipse vide -----------------------------------

void Draw_empty_ellipse_general(short center_x,short center_y,short horizontal_radius,short vertical_radius,byte color)
{
  short start_x;
  short start_y;
  short x_pos;
  short y_pos;

  start_x=center_x-horizontal_radius;
  start_y=center_y-vertical_radius;

  // Calcul des limites de l'ellipse
  Ellipse_compute_limites(horizontal_radius+1,vertical_radius+1);

  // Affichage des extremit�es de l'ellipse sur chaque quart de l'ellipse:
  for (y_pos=start_y,Ellipse_cursor_Y=-vertical_radius;y_pos<center_y;y_pos++,Ellipse_cursor_Y++)
    for (x_pos=start_x,Ellipse_cursor_X=-horizontal_radius;x_pos<center_x;x_pos++,Ellipse_cursor_X++)
      if (Pixel_in_ellipse())
      {
        // On vient de tomber sur le premier point qui sur la ligne
        // horizontale fait partie de l'ellipse.

        // Donc on peut l'afficher (lui et ses copains sym�triques)

         // Quart Haut-gauche
        Pixel_figure(x_pos,y_pos,color);
         // Quart Haut-droite
        Pixel_figure((center_x<<1)-x_pos,y_pos,color);
         // Quart Bas-gauche
        Pixel_figure(x_pos,(center_y<<1)-y_pos,color);
         // Quart Bas-droite
        Pixel_figure((center_x<<1)-x_pos,(center_y<<1)-y_pos,color);

        // On peut ensuite afficher tous les points qui le suivent dont le
        // pixel voisin du haut n'appartient pas � l'ellipse:
        for (Ellipse_cursor_Y--,x_pos++,Ellipse_cursor_X++;x_pos<center_x;x_pos++,Ellipse_cursor_X++)
          if (!Pixel_in_ellipse())
          {
             // Quart Haut-gauche
            Pixel_figure(x_pos,y_pos,color);
             // Quart Haut-droite
            Pixel_figure((center_x<<1)-x_pos,y_pos,color);
             // Quart Bas-gauche
            Pixel_figure(x_pos,(center_y<<1)-y_pos,color);
             // Quart Bas-droite
            Pixel_figure((center_x<<1)-x_pos,(center_y<<1)-y_pos,color);
          }
          else
            break;

        Ellipse_cursor_Y++;
        break;
      }

  // On affiche � la fin les points cardinaux:

  // points verticaux:
  x_pos=center_x;
  Ellipse_cursor_X=-1;
  for (y_pos=center_y+1-vertical_radius,Ellipse_cursor_Y=-vertical_radius+1;y_pos<center_y+vertical_radius;y_pos++,Ellipse_cursor_Y++)
    if (!Pixel_in_ellipse())
      Pixel_figure(x_pos,y_pos,color);

  // points horizontaux:
  y_pos=center_y;
  Ellipse_cursor_Y=-1;
  for (x_pos=center_x+1-horizontal_radius,Ellipse_cursor_X=-horizontal_radius+1;x_pos<center_x+horizontal_radius;x_pos++,Ellipse_cursor_X++)
    if (!Pixel_in_ellipse())
      Pixel_figure(x_pos,y_pos,color);

  Pixel_figure(center_x,center_y-vertical_radius,color);   // Haut
  Pixel_figure(center_x-horizontal_radius,center_y,color); // Gauche
  Pixel_figure(center_x+horizontal_radius,center_y,color); // Droite
  Pixel_figure(center_x,center_y+vertical_radius,color);   // Bas

  Update_part_of_screen(center_x-horizontal_radius,center_y-vertical_radius,2*horizontal_radius+1,2*vertical_radius+1);
}

  // -- Trac� d�finitif d'une ellipse vide --

void Draw_empty_ellipse_permanent(short center_x,short center_y,short horizontal_radius,short vertical_radius,byte color)
{
  Pixel_figure=Pixel_figure_permanent;
  Init_permanent_draw();
  Draw_empty_ellipse_general(center_x,center_y,horizontal_radius,vertical_radius,color);
  Update_part_of_screen(center_x - horizontal_radius, center_y - vertical_radius, 2* horizontal_radius+1, 2*vertical_radius+1);
}

  // -- Tracer la preview d'une ellipse vide --

void Draw_empty_ellipse_preview(short center_x,short center_y,short horizontal_radius,short vertical_radius,byte color)
{
  Pixel_figure=Pixel_figure_preview;
  Draw_empty_ellipse_general(center_x,center_y,horizontal_radius,vertical_radius,color);
  Update_part_of_screen(center_x - horizontal_radius, center_y - vertical_radius, 2* horizontal_radius+1, 2*vertical_radius +1);
}

  // -- Effacer la preview d'une ellipse vide --

void Hide_empty_ellipse_preview(short center_x,short center_y,short horizontal_radius,short vertical_radius)
{
  Pixel_figure=Pixel_figure_clear_preview;
  Draw_empty_ellipse_general(center_x,center_y,horizontal_radius,vertical_radius,0);
  Update_part_of_screen(center_x - horizontal_radius, center_y - vertical_radius, 2* horizontal_radius+1, 2*vertical_radius+1);
}

  // -- Tracer une ellipse pleine --

void Draw_filled_ellipse(short center_x,short center_y,short horizontal_radius,short vertical_radius,byte color)
{
  short start_x;
  short start_y;
  short x_pos;
  short y_pos;
  short end_x;
  short end_y;

  start_x=center_x-horizontal_radius;
  start_y=center_y-vertical_radius;
  end_x=center_x+horizontal_radius;
  end_y=center_y+vertical_radius;

  // Calcul des limites de l'ellipse
  Ellipse_compute_limites(horizontal_radius+1,vertical_radius+1);

  // Correction des bornes d'apr�s les limites
  if (start_y<Limit_top)
    start_y=Limit_top;
  if (end_y>Limit_bottom)
    end_y=Limit_bottom;
  if (start_x<Limit_left)
    start_x=Limit_left;
  if (end_x>Limit_right)
    end_x=Limit_right;

  // Affichage de l'ellipse
  for (y_pos=start_y,Ellipse_cursor_Y=start_y-center_y;y_pos<=end_y;y_pos++,Ellipse_cursor_Y++)
    for (x_pos=start_x,Ellipse_cursor_X=start_x-center_x;x_pos<=end_x;x_pos++,Ellipse_cursor_X++)
      if (Pixel_in_ellipse())
        Display_pixel(x_pos,y_pos,color);
  Update_part_of_screen(center_x-horizontal_radius,center_y-vertical_radius,2*horizontal_radius+1,2*vertical_radius+1);
}

/******************
* TRAC� DE LIGNES *
******************/

/// Alters bx and by so the (AX,AY)-(BX,BY) segment becomes either horizontal,
/// vertical, 45degrees, or isometrical for pixelart (ie 2:1 ratio)
void Clamp_coordinates_regular_angle(short ax, short ay, short* bx, short* by)
{
  int dx, dy;
  float angle;

  dx = *bx-ax;
  dy = *by-ay;

  // No mouse move: no need to clamp anything
  if (dx==0 || dy == 0) return;

  // Determine angle (heading)
  angle = atan2(dx, dy);

  // Get absolute values, useful from now on:
  //dx=abs(dx);
  //dy=abs(dy);

  // Negative Y
  if (angle < M_PI*(-15.0/16.0) || angle > M_PI*(15.0/16.0))
  {
    *bx=ax;
    *by=ay + dy;
  }
  // Iso close to negative Y
  else if (angle < M_PI*(-13.0/16.0))
  {
    dy=dy | 1; // Round up to next odd number
    *bx=ax + dy/2;
    *by=ay + dy;
  }
  // 45deg
  else if (angle < M_PI*(-11.0/16.0))
  {
    *by = (*by + ay + dx)/2;
    *bx = ax  - ay + *by;
  }
  // Iso close to negative X
  else if (angle < M_PI*(-9.0/16.0))
  {
    dx=dx | 1; // Round up to next odd number
    *bx=ax + dx;
    *by=ay + dx/2;
  }
  // Negative X
  else if (angle < M_PI*(-7.0/16.0))
  {
    *bx=ax + dx;
    *by=ay;
  }
  // Iso close to negative X
  else if (angle < M_PI*(-5.0/16.0))
  {
    dx=dx | 1; // Round up to next odd number
    *bx=ax + dx;
    *by=ay - dx/2;
  }
  // 45 degrees
  else if (angle < M_PI*(-3.0/16.0))
  {
    *by = (*by + ay - dx)/2;
    *bx = ax  + ay - *by;
  }
  // Iso close to positive Y
  else if (angle < M_PI*(-1.0/16.0))
  {
    dy=dy | 1; // Round up to next odd number
    *bx=ax - dy/2;
    *by=ay + dy;
  }
  // Positive Y
  else if (angle < M_PI*(1.0/16.0))
  {
    *bx=ax;
    *by=ay + dy;
  }
  // Iso close to positive Y
  else if (angle < M_PI*(3.0/16.0))
  {
    dy=dy | 1; // Round up to next odd number
    *bx=ax + dy/2;
    *by=ay + dy;
  }
  // 45 degrees
  else if (angle < M_PI*(5.0/16.0))
  {
    *by = (*by + ay + dx)/2;
    *bx = ax  - ay + *by;
  }
  // Iso close to positive X
  else if (angle < M_PI*(7.0/16.0))
  {
    dx=dx | 1; // Round up to next odd number
    *bx=ax + dx;
    *by=ay + dx/2;
  }
  // Positive X
  else if (angle < M_PI*(9.0/16.0))
  {
    *bx=ax + dx;
    *by=ay;
  }
  // Iso close to positive X
  else if (angle < M_PI*(11.0/16.0))
  {
    dx=dx | 1; // Round up to next odd number
    *bx=ax + dx;
    *by=ay - dx/2;
  }
  // 45 degrees
  else if (angle < M_PI*(13.0/16.0))
  {
    *by = (*by + ay - dx)/2;
    *bx = ax  + ay - *by;
  }
  // Iso close to negative Y
  else //if (angle < M_PI*(15.0/16.0))
  {
    dy=dy | 1; // Round up to next odd number
    *bx=ax - dy/2;
    *by=ay + dy;
  }

  return;
}

  // -- Tracer g�n�ral d'une ligne ------------------------------------------

void Draw_line_general(short start_x,short start_y,short end_x,short end_y, byte color)
{
  short x_pos,y_pos;
  short incr_x,incr_y;
  short i,cumul;
  short delta_x,delta_y;

  x_pos=start_x;
  y_pos=start_y;

  if (start_x<end_x)
  {
    incr_x=+1;
    delta_x=end_x-start_x;
  }
  else
  {
    incr_x=-1;
    delta_x=start_x-end_x;
  }

  if (start_y<end_y)
  {
    incr_y=+1;
    delta_y=end_y-start_y;
  }
  else
  {
    incr_y=-1;
    delta_y=start_y-end_y;
  }

  if (delta_y>delta_x)
  {
    cumul=delta_y>>1;
    for (i=1; i<delta_y; i++)
    {
      y_pos+=incr_y;
      cumul+=delta_x;
      if (cumul>=delta_y)
      {
        cumul-=delta_y;
        x_pos+=incr_x;
      }
      Pixel_figure(x_pos,y_pos,color);
    }
  }
  else
  {
    cumul=delta_x>>1;
    for (i=1; i<delta_x; i++)
    {
      x_pos+=incr_x;
      cumul+=delta_y;
      if (cumul>=delta_x)
      {
        cumul-=delta_x;
        y_pos+=incr_y;
      }
      Pixel_figure(x_pos,y_pos,color);
    }
  }

  if ( (start_x!=end_x) || (start_y!=end_y) )
    Pixel_figure(end_x,end_y,color);

}

  // -- Tracer d�finitif d'une ligne --

void Draw_line_permanent(short start_x,short start_y,short end_x,short end_y, byte color)
{

  int w = end_x-start_x, h = end_y - start_y;
  Pixel_figure=Pixel_figure_permanent;
  Init_permanent_draw();
  Draw_line_general(start_x,start_y,end_x,end_y,color);
  Update_part_of_screen((start_x<end_x)?start_x:end_x,(start_y<end_y)?start_y:end_y,abs(w)+1,abs(h)+1);
}

  // -- Tracer la preview d'une ligne --

void Draw_line_preview(short start_x,short start_y,short end_x,short end_y,byte color)
{
  int w = end_x-start_x, h = end_y - start_y;
  Pixel_figure=Pixel_figure_preview;
  Draw_line_general(start_x,start_y,end_x,end_y,color);
  Update_part_of_screen((start_x<end_x)?start_x:end_x,(start_y<end_y)?start_y:end_y,abs(w)+1,abs(h)+1);
}

  // -- Tracer la preview d'une ligne en xor --

void Draw_line_preview_xor(short start_x,short start_y,short end_x,short end_y,byte color)
{
  int w, h;

  Pixel_figure=(Func_pixel)Pixel_figure_preview_xor;
  // Needed a cast because this function supports signed shorts,
  // (it's usually in image space), while this time it's in screen space
  // and some line endpoints can be out of screen.
  Draw_line_general(start_x,start_y,end_x,end_y,color);

  if (start_x<Limit_left)
    start_x=Limit_left;
  if (start_y<Limit_top)
    start_y=Limit_top;
  if (end_x<Limit_left)
    end_x=Limit_left;
  if (end_y<Limit_top)
    end_y=Limit_top;
  // bottom & right limits are handled by Update_part_of_screen()

  w = end_x-start_x;
  h = end_y-start_y;
  Update_part_of_screen((start_x<end_x)?start_x:end_x,(start_y<end_y)?start_y:end_y,abs(w)+1,abs(h)+1);
}

  // -- Tracer la preview d'une ligne en xor additif --

void Draw_line_preview_xorback(short start_x,short start_y,short end_x,short end_y,byte color)
{
  int w = end_x-start_x, h = end_y - start_y;
  Pixel_figure=Pixel_figure_preview_xorback;
  Draw_line_general(start_x,start_y,end_x,end_y,color);
  Update_part_of_screen((start_x<end_x)?start_x:end_x,(start_y<end_y)?start_y:end_y,abs(w)+1,abs(h)+1);
}

  // -- Effacer la preview d'une ligne --

void Hide_line_preview(short start_x,short start_y,short end_x,short end_y)
{
  int w = end_x-start_x, h = end_y - start_y;
  Pixel_figure=Pixel_figure_clear_preview;
  Draw_line_general(start_x,start_y,end_x,end_y,0);
  Update_part_of_screen((start_x<end_x)?start_x:end_x,(start_y<end_y)?start_y:end_y,abs(w)+1,abs(h)+1);
}


  // -- Tracer un rectangle vide --

void Draw_empty_rectangle(short start_x,short start_y,short end_x,short end_y,byte color)
{
  short temp;
  short x_pos;
  short y_pos;


  // On v�rifie que les bornes soient dans le bon sens:
  if (start_x>end_x)
  {
    temp=start_x;
    start_x=end_x;
    end_x=temp;
  }
  if (start_y>end_y)
  {
    temp=start_y;
    start_y=end_y;
    end_y=temp;
  }

  // On trace le rectangle:
  Init_permanent_draw();

  for (x_pos=start_x;x_pos<=end_x;x_pos++)
  {
    Pixel_figure_permanent(x_pos,start_y,color);
    Pixel_figure_permanent(x_pos,  end_y,color);
  }

  for (y_pos=start_y+1;y_pos<end_y;y_pos++)
  {
    Pixel_figure_permanent(start_x,y_pos,color);
    Pixel_figure_permanent(  end_x,y_pos,color);
  }

#if defined(__macosx__) || defined(__FreeBSD__)
  Update_part_of_screen(start_x,end_x,end_x-start_x,end_y-start_y);
#endif
}

  // -- Tracer un rectangle plein --

void Draw_filled_rectangle(short start_x,short start_y,short end_x,short end_y,byte color)
{
  short temp;
  short x_pos;
  short y_pos;


  // On v�rifie que les bornes sont dans le bon sens:
  if (start_x>end_x)
  {
    temp=start_x;
    start_x=end_x;
    end_x=temp;
  }
  if (start_y>end_y)
  {
    temp=start_y;
    start_y=end_y;
    end_y=temp;
  }

  // Correction en cas de d�passement des limites de l'image
  if (end_x>Limit_right)
    end_x=Limit_right;
  if (end_y>Limit_bottom)
    end_y=Limit_bottom;

  // On trace le rectangle:
  for (y_pos=start_y;y_pos<=end_y;y_pos++)
    for (x_pos=start_x;x_pos<=end_x;x_pos++)
      // Display_pixel traite chaque pixel avec tous les effets ! (smear, ...)
      // Donc on ne peut pas otimiser en tra�ant ligne par ligne avec memset :(
      Display_pixel(x_pos,y_pos,color);
  Update_part_of_screen(start_x,start_y,end_x-start_x,end_y-start_y);

}




  // -- Tracer une courbe de B�zier --

void Draw_curve_general(short x1, short y1,
                           short x2, short y2,
                           short x3, short y3,
                           short x4, short y4,
                           byte color)
{
  float delta,t,t2,t3;
  short x,y,old_x,old_y;
  word  i;
  int   cx[4];
  int   cy[4];

  // Calcul des vecteurs de coefficients
  cx[0]= -   x1 + 3*x2 - 3*x3 + x4;
  cx[1]= + 3*x1 - 6*x2 + 3*x3;
  cx[2]= - 3*x1 + 3*x2;
  cx[3]= +   x1;
  cy[0]= -   y1 + 3*y2 - 3*y3 + y4;
  cy[1]= + 3*y1 - 6*y2 + 3*y3;
  cy[2]= - 3*y1 + 3*y2;
  cy[3]= +   y1;

  // Tra�age de la courbe
  old_x=x1;
  old_y=y1;
  Pixel_figure(old_x,old_y,color);
  delta=0.05; // 1.0/20
  t=0;
  for (i=1; i<=20; i++)
  {
    t=t+delta; t2=t*t; t3=t2*t;
    x=Round(t3*cx[0] + t2*cx[1] + t*cx[2] + cx[3]);
    y=Round(t3*cy[0] + t2*cy[1] + t*cy[2] + cy[3]);
    Draw_line_general(old_x,old_y,x,y,color);
    old_x=x;
    old_y=y;
  }

  x = Min(Min(x1,x2),Min(x3,x4));
  y = Min(Min(y1,y2),Min(y3,y4));
  old_x = Max(Max(x1,x2),Max(x3,x4)) - x;
  old_y = Max(Max(y1,y2),Max(y3,y4)) - y;
  Update_part_of_screen(x,y,old_x+1,old_y+1);
}

  // -- Tracer une courbe de B�zier d�finitivement --

void Draw_curve_permanent(short x1, short y1,
                             short x2, short y2,
                             short x3, short y3,
                             short x4, short y4,
                             byte color)
{
  Pixel_figure=Pixel_figure_permanent;
  Init_permanent_draw();
  Draw_curve_general(x1,y1,x2,y2,x3,y3,x4,y4,color);
}

  // -- Tracer la preview d'une courbe de B�zier --

void Draw_curve_preview(short x1, short y1,
                           short x2, short y2,
                           short x3, short y3,
                           short x4, short y4,
                           byte color)
{
  Pixel_figure=Pixel_figure_preview;
  Draw_curve_general(x1,y1,x2,y2,x3,y3,x4,y4,color);
}

  // -- Effacer la preview d'une courbe de B�zier --

void Hide_curve_preview(short x1, short y1,
                            short x2, short y2,
                            short x3, short y3,
                            short x4, short y4,
                            byte color)
{
  Pixel_figure=Pixel_figure_clear_preview;
  Draw_curve_general(x1,y1,x2,y2,x3,y3,x4,y4,color);
}




  // -- Spray : un petit coup de Pschiitt! --

void Airbrush(short clicked_button)
{
  short x_pos,y_pos;
  short radius=Airbrush_size>>1;
  long  radius_squared=(long)radius*radius;
  short index,count;
  byte  color_index;
  byte  direction;


  Hide_cursor();

  if (Airbrush_mode)
  {
    for (count=1; count<=Airbrush_mono_flow; count++)
    {
      x_pos=(rand()%Airbrush_size)-radius;
      y_pos=(rand()%Airbrush_size)-radius;
      if ( (x_pos*x_pos)+(y_pos*y_pos) <= radius_squared )
      {
        x_pos+=Paintbrush_X;
        y_pos+=Paintbrush_Y;
        if (clicked_button==1)
          Draw_paintbrush(x_pos,y_pos,Fore_color);
        else
          Draw_paintbrush(x_pos,y_pos,Back_color);
      }
    }
  }
  else
  {
    //   On essaye de se balader dans la table des flux de fa�on � ce que ce
    // ne soit pas toujours la derni�re couleur qui soit affich�e en dernier
    // Pour �a, on part d'une couleur au pif dans une direction al�atoire.
    direction=rand()&1;
    for (index=0,color_index=rand()/*%256*/; index<256; index++)
    {
      for (count=1; count<=Airbrush_multi_flow[color_index]; count++)
      {
        x_pos=(rand()%Airbrush_size)-radius;
        y_pos=(rand()%Airbrush_size)-radius;
        if ( (x_pos*x_pos)+(y_pos*y_pos) <= radius_squared )
        {
          x_pos+=Paintbrush_X;
          y_pos+=Paintbrush_Y;
          if (clicked_button==LEFT_SIDE)
            Draw_paintbrush(x_pos,y_pos,color_index);
          else
            Draw_paintbrush(x_pos,y_pos,Back_color);
        }
      }
      if (direction)
        color_index++;
      else
        color_index--;
    }
  }

  Display_cursor();
}



  //////////////////////////////////////////////////////////////////////////
  ////////////////////////// GESTION DES DEGRADES //////////////////////////
  //////////////////////////////////////////////////////////////////////////


  // -- Gestion d'un d�grad� de base (le plus moche) --

void Gradient_basic(long index,short x_pos,short y_pos)
{
  long position;

  // On fait un premier calcul partiel
  position=(index*Gradient_bounds_range);

  // On g�re un d�placement au hasard
  position+=(Gradient_total_range*(rand()%Gradient_random_factor)) >>6;
  position-=(Gradient_total_range*Gradient_random_factor) >>7;

  position/=Gradient_total_range;

  //   On va v�rifier que nos petites idioties n'ont pas �ject� la valeur hors
  // des valeurs autoris�es par le d�grad� d�fini par l'utilisateur.

  if (position<0)
    position=0;
  else if (position>=Gradient_bounds_range)
    position=Gradient_bounds_range-1;

  // On ram�ne ensuite la position dans le d�grad� vers un num�ro de couleur
  if (Gradient_is_inverted)
    Gradient_pixel(x_pos,y_pos,Gradient_upper_bound-position);
  else
    Gradient_pixel(x_pos,y_pos,Gradient_lower_bound+position);
}


  // -- Gestion d'un d�grad� par trames simples --

void Gradient_dithered(long index,short x_pos,short y_pos)
{
  long position_in_gradient;
  long position_in_segment;

  //
  //   But de l'op�ration: en plus de calculer la position de base (d�sign�e
  // dans cette proc�dure par "position_in_gradient", on calcule la position
  // de l'indice dans le sch�ma suivant:
  //
  //         | Les indices qui tra�nent de ce c�t� du segment se voient subir
  //         | une incr�mentation conditionnelle � leur position dans l'�cran.
  //         v
  //  |---|---|---|---- - - -
  //   ^
  //   |_ Les indices qui tra�nent de ce c�t� du segment se voient subir une
  //      d�cr�mentation conditionnelle � leur position dans l'�cran.

  // On fait d'abord un premier calcul partiel
  position_in_gradient=(index*Gradient_bounds_range);

  // On g�re un d�placement au hasard...
  position_in_gradient+=(Gradient_total_range*(rand()%Gradient_random_factor)) >>6;
  position_in_gradient-=(Gradient_total_range*Gradient_random_factor) >>7;

  if (position_in_gradient<0)
    position_in_gradient=0;

  // ... qui nous permet de calculer la position dans le segment
  position_in_segment=((position_in_gradient<<2)/Gradient_total_range)&3;

  // On peut ensuite terminer le calcul de l'indice dans le d�grad�
  position_in_gradient/=Gradient_total_range;

  // On va pouvoir discuter de la valeur de position_in_gradient en fonction
  // de la position dans l'�cran et de la position_in_segment.

  switch (position_in_segment)
  {
    case 0 : // On est sur la gauche du segment
      if (((x_pos+y_pos)&1)==0)
        position_in_gradient--;
      break;

      // On n'a pas � traiter les cas 1 et 2 car ils repr�sentent des valeurs
      // suffisament au centre du segment pour ne pas avoir � subir la trame

    case 3 : // On est sur la droite du segment
      if (((x_pos+y_pos)&1)!=0) // Note: on doit faire le test inverse au cas gauche pour synchroniser les 2 c�t�s de la trame.
        position_in_gradient++;
  }

  //   On va v�rifier que nos petites idioties n'ont pas �ject� la valeur hors
  // des valeurs autoris�es par le d�grad� d�fini par l'utilisateur.

  if (position_in_gradient<0)
    position_in_gradient=0;
  else if (position_in_gradient>=Gradient_bounds_range)
    position_in_gradient=Gradient_bounds_range-1;

  // On ram�ne ensuite la position dans le d�grad� vers un num�ro de couleur
  if (Gradient_is_inverted)
    position_in_gradient=Gradient_upper_bound-position_in_gradient;
  else
    position_in_gradient=Gradient_lower_bound+position_in_gradient;

  Gradient_pixel(x_pos,y_pos,position_in_gradient);
}


  // -- Gestion d'un d�grad� par trames �tendues --

void Gradient_extra_dithered(long index,short x_pos,short y_pos)
{
  long position_in_gradient;
  long position_in_segment;

//
  //   But de l'op�ration: en plus de calculer la position de base (d�sign�e
  // dans cette proc�dure par "position_in_gradient", on calcule la position
  // de l'indice dans le sch�ma suivant:
  //
  //         | Les indices qui tra�nent de ce c�t� du segment se voient subir
  //         | une incr�mentation conditionnelle � leur position dans l'�cran.
  //         v
  //  |---|---|---|---- - - -
  //   ^
  //   |_ Les indices qui tra�nent de ce c�t� du segment se voient subir une
  //      d�cr�mentation conditionnelle � leur position dans l'�cran.

  // On fait d'abord un premier calcul partiel
  position_in_gradient=(index*Gradient_bounds_range);

  // On g�re un d�placement au hasard
  position_in_gradient+=(Gradient_total_range*(rand()%Gradient_random_factor)) >>6;
  position_in_gradient-=(Gradient_total_range*Gradient_random_factor) >>7;

  if (position_in_gradient<0)
    position_in_gradient=0;

  // Qui nous permet de calculer la position dans le segment
  position_in_segment=((position_in_gradient<<3)/Gradient_total_range)&7;

  // On peut ensuite terminer le calcul de l'indice dans le d�grad�
  position_in_gradient/=Gradient_total_range;

  // On va pouvoir discuter de la valeur de position_in_gradient en fonction
  // de la position dans l'�cran et de la position_in_segment.

  switch (position_in_segment)
  {
    case 0 : // On est sur l'extr�me gauche du segment
      if (((x_pos+y_pos)&1)==0)
        position_in_gradient--;
      break;

    case 1 : // On est sur la gauche du segment
    case 2 : // On est sur la gauche du segment
      if (((x_pos & 1)==0) && ((y_pos & 1)==0))
        position_in_gradient--;
      break;

      // On n'a pas � traiter les cas 3 et 4 car ils repr�sentent des valeurs
      // suffisament au centre du segment pour ne pas avoir � subir la trame

    case 5 : // On est sur la droite du segment
    case 6 : // On est sur la droite du segment
      if (((x_pos & 1)==0) && ((y_pos & 1)!=0))
        position_in_gradient++;
      break;

    case 7 : // On est sur l'extreme droite du segment
      if (((x_pos+y_pos)&1)!=0) // Note: on doit faire le test inverse au cas gauche pour synchroniser les 2 c�t�s de la trame.
        position_in_gradient++;
  }

  //   On va v�rifier que nos petites idioties n'ont pas �ject� la valeur hors
  // des valeurs autoris�es par le d�grad� d�fini par l'utilisateur.

  if (position_in_gradient<0)
    position_in_gradient=0;
  else if (position_in_gradient>=Gradient_bounds_range)
    position_in_gradient=Gradient_bounds_range-1;

  // On ram�ne ensuite la position dans le d�grad� vers un num�ro de couleur
  if (Gradient_is_inverted)
    position_in_gradient=Gradient_upper_bound-position_in_gradient;
  else
    position_in_gradient=Gradient_lower_bound+position_in_gradient;

  Gradient_pixel(x_pos,y_pos,position_in_gradient);
}



  // -- Tracer un cercle degrad� (une sph�re) --

void Draw_grad_circle(short center_x,short center_y,short radius,short spot_x,short spot_y)
{
  long start_x;
  long start_y;
  long x_pos;
  long y_pos;
  long end_x;
  long end_y;
  long distance_x; // Distance (au carr�) sur les X du point en cours au centre d'�clairage
  long distance_y; // Distance (au carr�) sur les Y du point en cours au centre d'�clairage

  start_x=center_x-radius;
  start_y=center_y-radius;
  end_x=center_x+radius;
  end_y=center_y+radius;

  // Correction des bornes d'apr�s les limites
  if (start_y<Limit_top)
    start_y=Limit_top;
  if (end_y>Limit_bottom)
    end_y=Limit_bottom;
  if (start_x<Limit_left)
    start_x=Limit_left;
  if (end_x>Limit_right)
    end_x=Limit_right;

  Gradient_total_range=Circle_limit+
                           ((center_x-spot_x)*(center_x-spot_x))+
                           ((center_y-spot_y)*(center_y-spot_y))+
                           (2L*radius*sqrt(
                           ((center_x-spot_x)*(center_x-spot_x))+
                           ((center_y-spot_y)*(center_y-spot_y))));

  if (Gradient_total_range==0)
    Gradient_total_range=1;

  // Affichage du cercle
  for (y_pos=start_y,Circle_cursor_Y=(long)start_y-center_y;y_pos<=end_y;y_pos++,Circle_cursor_Y++)
  {
    distance_y =(y_pos-spot_y);
    distance_y*=distance_y;
    for (x_pos=start_x,Circle_cursor_X=(long)start_x-center_x;x_pos<=end_x;x_pos++,Circle_cursor_X++)
      if (Pixel_in_circle())
      {
        distance_x =(x_pos-spot_x);
        distance_x*=distance_x;
        Gradient_function(distance_x+distance_y,x_pos,y_pos);
      }
  }

  Update_part_of_screen(center_x-radius,center_y-radius,2*radius+1,2*radius+1);
}


  // -- Tracer une ellipse degrad�e --

void Draw_grad_ellipse(short center_x,short center_y,short horizontal_radius,short vertical_radius,short spot_x,short spot_y)
{
  long start_x;
  long start_y;
  long x_pos;
  long y_pos;
  long end_x;
  long end_y;
  long distance_x; // Distance (au carr�) sur les X du point en cours au centre d'�clairage
  long distance_y; // Distance (au carr�) sur les Y du point en cours au centre d'�clairage


  start_x=center_x-horizontal_radius;
  start_y=center_y-vertical_radius;
  end_x=center_x+horizontal_radius;
  end_y=center_y+vertical_radius;

  // Calcul des limites de l'ellipse
  Ellipse_compute_limites(horizontal_radius+1,vertical_radius+1);

  // On calcule la distance maximale:
  Gradient_total_range=(horizontal_radius*horizontal_radius)+
                           (vertical_radius*vertical_radius)+
                           ((center_x-spot_x)*(center_x-spot_x))+
                           ((center_y-spot_y)*(center_y-spot_y))+
                           (2L
                           *sqrt(
                           (horizontal_radius*horizontal_radius)+
                           (vertical_radius  *vertical_radius  ))
                           *sqrt(
                           ((center_x-spot_x)*(center_x-spot_x))+
                           ((center_y-spot_y)*(center_y-spot_y))));

  if (Gradient_total_range==0)
    Gradient_total_range=1;

  // Correction des bornes d'apr�s les limites
  if (start_y<Limit_top)
    start_y=Limit_top;
  if (end_y>Limit_bottom)
    end_y=Limit_bottom;
  if (start_x<Limit_left)
    start_x=Limit_left;
  if (end_x>Limit_right)
    end_x=Limit_right;

  // Affichage de l'ellipse
  for (y_pos=start_y,Ellipse_cursor_Y=start_y-center_y;y_pos<=end_y;y_pos++,Ellipse_cursor_Y++)
  {
    distance_y =(y_pos-spot_y);
    distance_y*=distance_y;
    for (x_pos=start_x,Ellipse_cursor_X=start_x-center_x;x_pos<=end_x;x_pos++,Ellipse_cursor_X++)
      if (Pixel_in_ellipse())
      {
        distance_x =(x_pos-spot_x);
        distance_x*=distance_x;
        Gradient_function(distance_x+distance_y,x_pos,y_pos);
      }
  }

  Update_part_of_screen(start_x,start_y,end_x-start_x+1,end_y-start_y+1);
}


// Trac� d'un rectangle (rax ray - rbx rby) d�grad� selon le vecteur (vax vay - vbx - vby)
void Draw_grad_rectangle(short rax,short ray,short rbx,short rby,short vax,short vay, short vbx, short vby)
{
    short y_pos, x_pos;

    // On commence par s'assurer que le rectangle est � l'endroit
    if(rbx < rax)
    {
      x_pos = rbx;
      rbx = rax;
      rax = x_pos;
    }

    if(rby < ray)
    {
      y_pos = rby;
      rby = ray;
      ray = y_pos;
    }

    // Correction des bornes d'apr�s les limites
    if (ray<Limit_top)
      ray=Limit_top;
    if (rby>Limit_bottom)
      rby=Limit_bottom;
    if (rax<Limit_left)
      rax=Limit_left;
    if (rbx>Limit_right)
      rbx=Limit_right;

    if(vbx == vax)
    {
      // Le vecteur est vertical, donc on �vite la partie en dessous qui foirerait avec une division par 0...
      if (vby == vay) return;  // L'utilisateur fait n'importe quoi
      Gradient_total_range = abs(vby - vay);
      for(y_pos=ray;y_pos<=rby;y_pos++)
        for(x_pos=rax;x_pos<=rbx;x_pos++)
          Gradient_function(abs(vby - y_pos),x_pos,y_pos);

    }
    else
    {
      float a;
      float b;
      float distance_x, distance_y;

      Gradient_total_range = sqrt(pow(vby - vay,2)+pow(vbx - vax,2));
      a = (float)(vby - vay)/(float)(vbx - vax);
      b = vay - a*vax;

      for (y_pos=ray;y_pos<=rby;y_pos++)
        for (x_pos = rax;x_pos<=rbx;x_pos++)
        {
          // On calcule ou on en est dans le d�grad�
          distance_x = pow((y_pos - vay),2)+pow((x_pos - vax),2);
          distance_y = pow((-a * x_pos + y_pos - b),2)/(a*a+1);

          Gradient_function((int)sqrt(distance_x - distance_y),x_pos,y_pos);
        }
    }
    Update_part_of_screen(rax,ray,rbx,rby);
}




// -- Tracer un polyg�ne plein --

typedef struct T_Polygon_edge      /* an active edge */
{
    short top;                     /* top y position */
    short bottom;                  /* bottom y position */
    float x, dx;                   /* floating point x position and gradient */
    float w;                       /* width of line segment */
    struct T_Polygon_edge *prev;     /* doubly linked list */
    struct T_Polygon_edge *next;
} T_Polygon_edge;



/* Fill_edge_structure:
 *  Polygon helper function: initialises an edge structure for the 2d
 *  rasteriser.
 */
void Fill_edge_structure(T_Polygon_edge *edge, short *i1, short *i2)
{
  short *it;

  if (i2[1] < i1[1])
  {
    it = i1;
    i1 = i2;
    i2 = it;
  }

  edge->top = i1[1];
  edge->bottom = i2[1] - 1;
  edge->dx = ((float) i2[0] - (float) i1[0]) / ((float) i2[1] - (float) i1[1]);
  edge->x = i1[0] + 0.4999999;
  edge->prev = NULL;
  edge->next = NULL;

  if (edge->dx+1 < 0.0)
    edge->x += edge->dx+1;

  if (edge->dx >= 0.0)
    edge->w = edge->dx;
  else
    edge->w = -(edge->dx);

  if (edge->w-1.0<0.0)
    edge->w = 0.0;
  else
    edge->w = edge->w-1;
}



/* Add_edge:
 *  Adds an edge structure to a linked list, returning the new head pointer.
 */
T_Polygon_edge * Add_edge(T_Polygon_edge *list, T_Polygon_edge *edge, int sort_by_x)
{
  T_Polygon_edge *pos = list;
  T_Polygon_edge *prev = NULL;

  if (sort_by_x)
  {
    while ( (pos) && ((pos->x+((pos->w+pos->dx)/2)) < (edge->x+((edge->w+edge->dx)/2))) )
    {
      prev = pos;
      pos = pos->next;
    }
  }
  else
  {
    while ((pos) && (pos->top < edge->top))
    {
      prev = pos;
      pos = pos->next;
    }
  }

  edge->next = pos;
  edge->prev = prev;

  if (pos)
    pos->prev = edge;

  if (prev)
  {
    prev->next = edge;
    return list;
  }
  else
    return edge;
}



/* Remove_edge:
 *  Removes an edge structure from a list, returning the new head pointer.
 */
T_Polygon_edge * Remove_edge(T_Polygon_edge *list, T_Polygon_edge *edge)
{
  if (edge->next)
    edge->next->prev = edge->prev;

  if (edge->prev)
  {
    edge->prev->next = edge->next;
    return list;
  }
  else
    return edge->next;
}



/* polygon:
 *  Draws a filled polygon with an arbitrary number of corners. Pass the
 *  number of vertices, then an array containing a series of x, y points
 *  (a total of vertices*2 values).
 */
void Polyfill_general(int vertices, short * points, int color)
{
  short c;
  short top = 0x7FFF;
  short bottom = 0;
  short *i1, *i2;
  short x_pos,end_x;
  T_Polygon_edge *edge, *next_edge, *initial_edge;
  T_Polygon_edge *active_edges = NULL;
  T_Polygon_edge *inactive_edges = NULL;

  /* allocate some space and fill the edge table */
  initial_edge=edge=(T_Polygon_edge *) malloc(sizeof(T_Polygon_edge) * vertices);

  i1 = points;
  i2 = points + ((vertices-1)<<1);

  for (c=0; c<vertices; c++)
  {
    if (i1[1] != i2[1])
    {
      Fill_edge_structure(edge, i1, i2);

      if (edge->bottom >= edge->top)
      {
        if (edge->top < top)
          top = edge->top;

        if (edge->bottom > bottom)
          bottom = edge->bottom;

        inactive_edges = Add_edge(inactive_edges, edge, 0);
        edge++;
      }
    }
    i2 = i1;
    i1 += 2;
  }

  /* for each scanline in the polygon... */
  for (c=top; c<=bottom; c++)
  {
    /* check for newly active edges */
    edge = inactive_edges;
    while ((edge) && (edge->top == c))
    {
      next_edge = edge->next;
      inactive_edges = Remove_edge(inactive_edges, edge);
      active_edges = Add_edge(active_edges, edge, 1);
      edge = next_edge;
    }

    /* draw horizontal line segments */
    if ((c>=Limit_top) && (c<=Limit_bottom))
    {
      edge = active_edges;
      while ((edge) && (edge->next))
      {
        x_pos=/*Round*/(edge->x);
        end_x=/*Round*/(edge->next->x+edge->next->w);
        if (x_pos<Limit_left)
          x_pos=Limit_left;
        if (end_x>Limit_right)
          end_x=Limit_right;
        for (; x_pos<=end_x; x_pos++)
          Pixel_figure(x_pos,c,color);
        edge = edge->next->next;
      }
    }

    /* update edges, sorting and removing dead ones */
    edge = active_edges;
    while (edge)
    {
      next_edge = edge->next;
      if (c >= edge->bottom)
        active_edges = Remove_edge(active_edges, edge);
      else
      {
        edge->x += edge->dx;
        while ((edge->prev) && ( (edge->x+(edge->w/2)) < (edge->prev->x+(edge->prev->w/2))) )
        {
          if (edge->next)
            edge->next->prev = edge->prev;
          edge->prev->next = edge->next;
          edge->next = edge->prev;
          edge->prev = edge->prev->prev;
          edge->next->prev = edge;
          if (edge->prev)
            edge->prev->next = edge;
          else
            active_edges = edge;
        }
      }
      edge = next_edge;
    }
  }

  free(initial_edge);
  initial_edge = NULL;

  // On ne connait pas simplement les xmin et xmax ici, mais de toutes fa�on ce n'est pas utilis� en preview
  Update_part_of_screen(0,top,Main_image_width,bottom-top+1);
}


void Polyfill(int vertices, short * points, int color)
{
  int index;

  Pixel_clipped(points[0],points[1],color);
  if (vertices==1)
  {
    Update_part_of_screen(points[0],points[1],1,1);
    return;
  }

  // Comme pour le Fill, cette operation fait un peu d'"overdraw"
  // (pixels dessin�s plus d'une fois) alors on force le FX Feedback � OFF
  Update_FX_feedback(0);

  Pixel_figure=Pixel_clipped;
  Polyfill_general(vertices,points,color);

  // Remarque: pour dessiner la bordure avec la brosse en cours au lieu
  // d'un pixel de couleur premier-plan, il suffit de mettre ici:
  // Pixel_figure=Pixel_figure_permanent;

  // Dessin du contour
  for (index=0; index<vertices-1;index+=1)
    Draw_line_general(points[index*2],points[index*2+1],points[index*2+2],points[index*2+3],color);
  Draw_line_general(points[0],points[1],points[index*2],points[index*2+1],color);

  // Restore original feedback value
  Update_FX_feedback(Config.FX_Feedback);

}



//------------ Remplacement de la couleur point�e par une autre --------------

void Replace(byte new_color)
{
  byte old_color;

  if ((Paintbrush_X<Main_image_width)
   && (Paintbrush_Y<Main_image_height))
  {
    old_color=Read_pixel_from_current_layer(Paintbrush_X,Paintbrush_Y);
    if ( (old_color!=new_color)
      && ((!Stencil_mode) || (!Stencil[old_color])) )
    {
      word x;
      word y;

      // Update all pixels
      for (y=0; y<Main_image_height; y++)
        for (x=0; x<Main_image_width; x++)
          if (Read_pixel_from_current_layer(x,y) == old_color)
            Pixel_in_current_screen(x,y,new_color);
    }
  }
}



/******************************************************************************/
/********************************** SHADES ************************************/

// Transformer une liste de shade en deux tables de conversion
void Shade_list_to_lookup_tables(word * list,short step,byte mode,byte * table_inc,byte * table_dec)
{
  int index;
  int first;
  int last;
  int color;
  int temp;


  // On initialise les deux tables de conversion en Identit�
  for (index=0;index<256;index++)
  {
    table_inc[index]=index;
    table_dec[index]=index;
  }

  // On s'appr�te � examiner l'ensemble de la liste
  for (index=0;index<512;index++)
  {
    // On recherche la premi�re case de la liste non vide (et non inhib�e)
    while ((index<512) && (list[index]>255))
      index++;

    // On note la position de la premi�re case de la s�quence
    first=index;

    // On recherche la position de la derni�re case de la s�quence
    for (last=first;list[last+1]<256;last++);

    // Pour toutes les cases non vides (et non inhib�es) qui suivent
    switch (mode)
    {
      case SHADE_MODE_NORMAL :
        for (;(index<512) && (list[index]<256);index++)
        { // On met � jour les tables de conversion
          color=list[index];
          table_inc[color]=list[(index+step<=last)?index+step:last];
          table_dec[color]=list[(index-step>=first)?index-step:first];
        }
        break;
      case SHADE_MODE_LOOP :
        temp=1+last-first;
        for (;(index<512) && (list[index]<256);index++)
        { // On met � jour les tables de conversion
          color=list[index];
          table_inc[color]=list[first+((step+index-first)%temp)];
          table_dec[color]=list[first+(((temp-step)+index-first)%temp)];
        }
        break;
      default : // SHADE_MODE_NOSAT
        for (;(index<512) && (list[index]<256);index++)
        { // On met � jour les tables de conversion
          color=list[index];
          if (index+step<=last)
            table_inc[color]=list[index+step];
          if (index-step>=first)
            table_dec[color]=list[index-step];
        }
    }
  }
}



// -- Interface avec l'image, affect�e par le facteur de grossissement -------

  // fonction d'affichage "Pixel" utilis�e pour les op�rations d�finitivement
  // Ne doit � aucune condition �tre appel�e en dehors de la partie visible
  // de l'image dans l'�cran (�a pourrait �tre grave)
void Display_pixel(word x,word y,byte color)
  // x & y    sont la position d'un point dans l'IMAGE
  // color  est la couleur du point
  // Le Stencil est g�r�.
  // Les effets sont g�r�s par appel � Effect_function().
  // La Loupe est g�r�e par appel � Pixel_preview().
{
  if ( ( (!Sieve_mode)   || (Effect_sieve(x,y)) )
    && (!((Stencil_mode) && (Stencil[Read_pixel_from_current_layer(x,y)])))
    && (!((Mask_mode)    && (Mask_table[Read_pixel_from_spare_screen(x,y)]))) )
  {
    color=Effect_function(x,y,color);
    if (Main_tilemap_mode)
    {
      Tilemap_draw(x,y, color);
    }
    else
      Pixel_in_current_screen_with_preview(x,y,color);
  }
}



// -- Calcul des diff�rents effets -------------------------------------------

  // -- Aucun effet en cours --

byte No_effect(word x, word y, byte color)
{
  (void)x; // unused
  (void)y; // unused

  return color;
}

  // -- Effet de Shading --

byte Effect_shade(word x,word y,byte color)
{
  (void)color; // unused

  return Shade_table[Read_pixel_from_feedback_screen(x,y)];
}

byte Effect_quick_shade(word x,word y,byte color)
{
  int c=color=Read_pixel_from_feedback_screen(x,y);
  int direction=(Fore_color<=Back_color);
  byte start,end;
  int width;

  if (direction)
  {
    start=Fore_color;
    end  =Back_color;
  }
  else
  {
    start=Back_color;
    end  =Fore_color;
  }

  if ((c>=start) && (c<=end) && (start!=end))
  {
    width=1+end-start;

    if ( ((Shade_table==Shade_table_left) && direction) || ((Shade_table==Shade_table_right) && (!direction)) )
      c-=Quick_shade_step%width;
    else
      c+=Quick_shade_step%width;

    if (c<start)
      switch (Quick_shade_loop)
      {
        case SHADE_MODE_NORMAL : return start;
        case SHADE_MODE_LOOP : return (width+c);
        default : return color;
      }

    if (c>end)
      switch (Quick_shade_loop)
      {
        case SHADE_MODE_NORMAL : return end;
        case SHADE_MODE_LOOP : return (c-width);
        default : return color;
      }
  }

  return c;
}

  // -- Effet de Tiling --

byte Effect_tiling(word x,word y,byte color)
{
  (void)color; // unused

  return Read_pixel_from_brush((x+Brush_width-Tiling_offset_X)%Brush_width,
                               (y+Brush_height-Tiling_offset_Y)%Brush_height);
}

  // -- Effet de Smooth --

byte Effect_smooth(word x,word y,byte color)
{
  int r,g,b;
  byte c;
  int weight,total_weight;
  byte x2=((x+1)<Main_image_width);
  byte y2=((y+1)<Main_image_height);
  (void)color; // unused

  // On commence par le pixel central
  c=Read_pixel_from_feedback_screen(x,y);
  total_weight=Smooth_matrix[1][1];
  r=total_weight*Main_palette[c].R;
  g=total_weight*Main_palette[c].G;
  b=total_weight*Main_palette[c].B;

  if (x)
  {
    c=Read_pixel_from_feedback_screen(x-1,y);
    total_weight+=(weight=Smooth_matrix[0][1]);
    r+=weight*Main_palette[c].R;
    g+=weight*Main_palette[c].G;
    b+=weight*Main_palette[c].B;

    if (y)
    {
      c=Read_pixel_from_feedback_screen(x-1,y-1);
      total_weight+=(weight=Smooth_matrix[0][0]);
      r+=weight*Main_palette[c].R;
      g+=weight*Main_palette[c].G;
      b+=weight*Main_palette[c].B;

      if (y2)
      {
        c=Read_pixel_from_feedback_screen(x-1,y+1);
        total_weight+=(weight=Smooth_matrix[0][2]);
        r+=weight*Main_palette[c].R;
        g+=weight*Main_palette[c].G;
        b+=weight*Main_palette[c].B;
      }
    }
  }

  if (x2)
  {
    c=Read_pixel_from_feedback_screen(x+1,y);
    total_weight+=(weight=Smooth_matrix[2][1]);
    r+=weight*Main_palette[c].R;
    g+=weight*Main_palette[c].G;
    b+=weight*Main_palette[c].B;

    if (y)
    {
      c=Read_pixel_from_feedback_screen(x+1,y-1);
      total_weight+=(weight=Smooth_matrix[2][0]);
      r+=weight*Main_palette[c].R;
      g+=weight*Main_palette[c].G;
      b+=weight*Main_palette[c].B;

      if (y2)
      {
        c=Read_pixel_from_feedback_screen(x+1,y+1);
        total_weight+=(weight=Smooth_matrix[2][2]);
        r+=weight*Main_palette[c].R;
        g+=weight*Main_palette[c].G;
        b+=weight*Main_palette[c].B;
      }
    }
  }

  if (y)
  {
    c=Read_pixel_from_feedback_screen(x,y-1);
    total_weight+=(weight=Smooth_matrix[1][0]);
    r+=weight*Main_palette[c].R;
    g+=weight*Main_palette[c].G;
    b+=weight*Main_palette[c].B;
  }

  if (y2)
  {
    c=Read_pixel_from_feedback_screen(x,y+1);
    total_weight+=(weight=Smooth_matrix[1][2]);
    r+=weight*Main_palette[c].R;
    g+=weight*Main_palette[c].G;
    b+=weight*Main_palette[c].B;
  }

  return (total_weight)? // On regarde s'il faut �viter le 0/0.
    Best_color(Round_div(r,total_weight),
                      Round_div(g,total_weight),
                      Round_div(b,total_weight)):
    Read_pixel_from_current_screen(x,y); // C'est bien l'�cran courant et pas
                                       // l'�cran feedback car il s'agit de ne
}                                      // pas modifier l'�cran courant.

byte Effect_layer_copy(word x,word y,byte color)
{
  if (color<Main_backups->Pages->Nb_layers)
  {
    return *((y)*Main_image_width+(x)+Main_backups->Pages->Image[color].Pixels);
  }
  return Read_pixel_from_feedback_screen(x,y);
}

byte Read_pixel_from_current_screen  (word x,word y)
{

  byte depth;
  byte color;

  if (Main_backups->Pages->Image_mode == IMAGE_MODE_ANIMATION)
  {
    return *((y)*Main_image_width+(x)+Main_backups->Pages->Image[Main_current_layer].Pixels);
  }

  if (Main_backups->Pages->Image_mode == IMAGE_MODE_MODE5)
    if (Main_current_layer==4)
      return *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width);

  color = *(Main_screen+y*Main_image_width+x);
  if (color != Main_backups->Pages->Transparent_color) // transparent color
    return color;

  depth = *(Main_visible_image_depth_buffer.Image+x+y*Main_image_width);
  return *(Main_backups->Pages->Image[depth].Pixels + x+y*Main_image_width);
}

/// Paint a a single pixel in image only : as-is.
void Pixel_in_screen_direct(word x,word y,byte color)
{
  *((y)*Main_image_width+(x)+Main_backups->Pages->Image[Main_current_layer].Pixels)=color;
}

/// Paint a a single pixel in image and on screen: as-is.
void Pixel_in_screen_direct_with_preview(word x,word y,byte color)
{
  *((y)*Main_image_width+(x)+Main_backups->Pages->Image[Main_current_layer].Pixels)=color;
  Pixel_preview(x,y,color);
}

/// Paint a a single pixel in image only : using layered display.
void Pixel_in_screen_layered(word x,word y,byte color)
{
  byte depth = *(Main_visible_image_depth_buffer.Image+x+y*Main_image_width);
  *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
  if ( depth <= Main_current_layer)
  {
    if (color == Main_backups->Pages->Transparent_color) // transparent color
      // fetch pixel color from the topmost visible layer
      color=*(Main_backups->Pages->Image[depth].Pixels + x+y*Main_image_width);

    *(x+y*Main_image_width+Main_screen)=color;
  }
}

/// Paint a a single pixel in image and on screen : using layered display.
void Pixel_in_screen_layered_with_preview(word x,word y,byte color)
{
  byte depth = *(Main_visible_image_depth_buffer.Image+x+y*Main_image_width);
  *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
  if ( depth <= Main_current_layer)
  {
    if (color == Main_backups->Pages->Transparent_color) // transparent color
      // fetch pixel color from the topmost visible layer
      color=*(Main_backups->Pages->Image[depth].Pixels + x+y*Main_image_width);

    *(x+y*Main_image_width+Main_screen)=color;

    Pixel_preview(x,y,color);
  }
}

/// Paint a a single pixel in image only : in a layer under one that acts as a layer-selector (mode 5).
void Pixel_in_screen_underlay(word x,word y,byte color)
{
  byte depth;

  // Paste in layer
  *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
  // Search depth
  depth = *(Main_backups->Pages->Image[4].Pixels + x+y*Main_image_width);

  if ( depth == Main_current_layer)
  {
    // Draw that color on the visible image buffer
    *(x+y*Main_image_width+Main_screen)=color;
  }
}

/// Paint a a single pixel in image and on screen : in a layer under one that acts as a layer-selector (mode 5).
void Pixel_in_screen_underlay_with_preview(word x,word y,byte color)
{
  byte depth;

  // Paste in layer
  *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
  // Search depth
  depth = *(Main_backups->Pages->Image[4].Pixels + x+y*Main_image_width);

  if ( depth == Main_current_layer)
  {
    // Draw that color on the visible image buffer
    *(x+y*Main_image_width+Main_screen)=color;

    Pixel_preview(x,y,color);
  }
}

/// Paint a a single pixel in image only : in a layer that acts as a layer-selector (mode 5).
void Pixel_in_screen_overlay(word x,word y,byte color)
{
  if (color<4)
  {
    // Paste in layer
    *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
    // Paste in depth buffer
    *(Main_visible_image_depth_buffer.Image+x+y*Main_image_width)=color;
    // Fetch pixel color from the target raster layer
	if (Main_layers_visible & (1 << color))
    	color=*(Main_backups->Pages->Image[color].Pixels + x+y*Main_image_width);
    // Draw that color on the visible image buffer
    *(x+y*Main_image_width+Main_screen)=color;
  }
}

/// Paint a a single pixel in image and on screen : in a layer that acts as a layer-selector (mode 5).
void Pixel_in_screen_overlay_with_preview(word x,word y,byte color)
{
  if (color<4)
  {
    // Paste in layer
    *(Main_backups->Pages->Image[Main_current_layer].Pixels + x+y*Main_image_width)=color;
    // Paste in depth buffer
    *(Main_visible_image_depth_buffer.Image+x+y*Main_image_width)=color;
    // Fetch pixel color from the target raster layer
	if (Main_layers_visible & (1 << color))
    	color=*(Main_backups->Pages->Image[color].Pixels + x+y*Main_image_width);
    // Draw that color on the visible image buffer
    *(x+y*Main_image_width+Main_screen)=color;

    Pixel_preview(x,y,color);
  }
}

Func_pixel Pixel_in_current_screen=Pixel_in_screen_direct;
Func_pixel Pixel_in_current_screen_with_preview=Pixel_in_screen_direct_with_preview;

void Pixel_in_spare(word x,word y, byte color)
{
  *((y)*Spare_image_width+(x)+Spare_backups->Pages->Image[Spare_current_layer].Pixels)=color;
}

void Pixel_in_current_layer(word x,word y, byte color)
{
  *((y)*Main_image_width+(x)+Main_backups->Pages->Image[Main_current_layer].Pixels)=color;
}

byte Read_pixel_from_current_layer(word x,word y)
{
  return *((y)*Main_image_width+(x)+Main_backups->Pages->Image[Main_current_layer].Pixels);
}

void Update_pixel_renderer(void)
{
  if (Main_backups->Pages->Image_mode == IMAGE_MODE_ANIMATION)
  {
    // direct
    Pixel_in_current_screen = Pixel_in_screen_direct;
    Pixel_in_current_screen_with_preview = Pixel_in_screen_direct_with_preview;
  }
  else
  if (Main_backups->Pages->Image_mode == IMAGE_MODE_LAYERED)
  {
    // layered
    Pixel_in_current_screen = Pixel_in_screen_layered;
    Pixel_in_current_screen_with_preview = Pixel_in_screen_layered_with_preview;
  }
  // Implicit else : Image_mode must be IMAGE_MODE_MODE5
  else if ( Main_current_layer == 4)
  {
    // overlay
    Pixel_in_current_screen = Pixel_in_screen_overlay;
    Pixel_in_current_screen_with_preview = Pixel_in_screen_overlay_with_preview;
  }
  else if (Main_current_layer<4 && (Main_layers_visible & (1<<4)))
  {
    // underlay
    Pixel_in_current_screen = Pixel_in_screen_underlay;
    Pixel_in_current_screen_with_preview = Pixel_in_screen_underlay_with_preview;
  }
  else
  {
    // layered (again)
    Pixel_in_current_screen = Pixel_in_screen_layered;
    Pixel_in_current_screen_with_preview = Pixel_in_screen_layered_with_preview;
  }
}
