/*  Grafx2 - The Ultimate 256-color bitmap paint program

    Copyright 2008 Yves Rizoud
    Copyright 2008 Adrien Destugues
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
    along with Grafx2; if not, see <http://www.gnu.org/licenses/> or
    write to the Free Software Foundation, Inc.,
    59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// Pour d�sactiver le support TrueType, d�finir NOTTF
// To disable TrueType support, define NOTTF

#include <string.h>
#include <stdlib.h>

// TrueType
#ifndef NOTTF
#ifdef __macosx__
#include <SDL_ttf/SDL_ttf.h>
#else
#include <SDL/SDL_ttf.h>
#endif
#endif
#include <SDL/SDL_image.h>
// SFont
#include "SFont.h"

#include "struct.h"
#include "global.h"
#include "sdlscreen.h"
#include "io.h"

typedef struct T_FONTE
{
  char * Nom;
  int    EstTrueType;
  int    EstImage;
  
  // Liste chain�e simple
  struct T_FONTE * Suivante;
} T_FONTE;
// Liste chain�e des polices de texte
T_FONTE * Liste_fontes_debut;
T_FONTE * Liste_fontes_fin;
int Fonte_nombre;

// Ajout d'une fonte � la liste.
void Ajout_fonte(char *Nom, int EstTrueType, int EstImage)
{
  T_FONTE * Fonte = (T_FONTE *)malloc(sizeof(T_FONTE));
  Fonte->Nom = (char *)malloc(strlen(Nom)+1);
  strcpy(Fonte->Nom, Nom);  
  Fonte->EstTrueType = EstTrueType;
  Fonte->EstImage = EstImage;

  // Gestion Liste
  Fonte->Suivante = NULL;
  if (Liste_fontes_debut==NULL)
    Liste_fontes_debut = Fonte;
  else
    Liste_fontes_fin->Suivante = Fonte;
  Liste_fontes_fin = Fonte;
  Fonte_nombre++;

}

// Trouve le nom d'une fonte par son num�ro
char * Nom_fonte(int Indice)
{
  T_FONTE *Fonte = Liste_fontes_debut;
  if (Indice<0 ||Indice>=Fonte_nombre)
    return "";
  while (Indice--)
    Fonte = Fonte->Suivante;
  return Fonte->Nom;
}
// Trouve le libell� d'affichage d'une fonte par son num�ro
char * Libelle_fonte(int Indice)
{
  T_FONTE *Fonte;
  static char Libelle[22];
  char * Nom_fonte;
  
  strcpy(Libelle, "                     ");
  
  // Recherche de la fonte
  Fonte = Liste_fontes_debut;
  if (Indice<0 ||Indice>=Fonte_nombre)
    return Libelle;
  while (Indice--)
    Fonte = Fonte->Suivante;
  
  // Libell�
  if (Fonte->EstTrueType)
    Libelle[19]=Libelle[20]='T'; // Logo TT
  Nom_fonte=Position_dernier_slash(Fonte->Nom);
  if (Nom_fonte==NULL)
    Nom_fonte=Fonte->Nom;
  else
    Nom_fonte++;
  for (Indice=0; Indice < 19 && Nom_fonte[Indice]!='\0' && Nom_fonte[Indice]!='.'; Indice++)
    Libelle[Indice]=Nom_fonte[Indice];
  return Libelle;
}


// Initialisation � faire une fois au d�but du programme
void Initialisation_Texte(void)
{

  #ifndef NOTTF
  // Initialisation de TTF
  TTF_Init();
  
  // Initialisation des fontes
  Liste_fontes_debut = Liste_fontes_fin = NULL;
  Fonte_nombre=0;
  // Parcours du r�pertoire "fontes"
  Ajout_fonte("fonts/Tuffy.ttf", 1, 0);
  Ajout_fonte("fonts/Otherfont.ttf", 1, 0);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("8pxfont.png", 0, 1);
  Ajout_fonte("fonts/Tuffy.ttf", 1, 0);
  #endif
}

// Informe si texte.c a �t� compil� avec l'option de support TrueType ou pas.
int Support_TrueType()
{
  #ifdef NOTTF
  return 0;
  #else
  return 1;
  #endif
}

  
#ifndef NOTTF
byte *Rendu_Texte_TTF(const char *Chaine, int Taille, int AntiAlias, int *Largeur, int *Hauteur)
{
 TTF_Font *Fonte;
  SDL_Surface * TexteColore;
  SDL_Surface * Texte8Bit;
  byte * BrosseRetour;
  int Indice;
  SDL_Color PaletteSDL[256];
  
  SDL_Color Couleur_Avant;
  SDL_Color Couleur_Arriere;

  // Chargement de la fonte
  Fonte=TTF_OpenFont(Nom_fonte(0), Taille); // FIXME fonte en dur
  if (!Fonte)
  {
    return NULL;
  }
  // Couleurs
  Couleur_Avant = Conversion_couleur_SDL(Fore_color);
  Couleur_Arriere = Conversion_couleur_SDL(Back_color);
  
  // Rendu du texte: cr�e une surface SDL RGB 24bits
  if (AntiAlias)
    TexteColore=TTF_RenderText_Shaded(Fonte, Chaine, Couleur_Avant, Couleur_Arriere );
  else
    TexteColore=TTF_RenderText_Solid(Fonte, Chaine, Couleur_Avant);
  if (!TexteColore)
  {
    TTF_CloseFont(Fonte);
    return NULL;
  }
  
  Texte8Bit=SDL_DisplayFormat(TexteColore);
  /*
  
  Texte8Bit=SDL_CreateRGBSurface(SDL_SWSURFACE, TexteColore->w, TexteColore->h, 8, 0, 0, 0, 0);
  if (!Texte8Bit)
  {
    SDL_FreeSurface(TexteColore);
    TTF_CloseFont(Fonte);
    return NULL;
  }

  for(Indice=0;Indice<256;Indice++)
  {
    PaletteSDL[Indice].r=(Principal_Palette[Indice].R<<2) + (Principal_Palette[Indice].R>>4); //Les couleurs VGA ne vont que de 0 � 63
    PaletteSDL[Indice].g=(Principal_Palette[Indice].V<<2) + (Principal_Palette[Indice].V>>4);
    PaletteSDL[Indice].b=(Principal_Palette[Indice].B<<2) + (Principal_Palette[Indice].B>>4);
  }
  SDL_SetPalette(Texte8Bit,SDL_PHYSPAL|SDL_LOGPAL,PaletteSDL,0,256);
  SDL_BlitSurface(TexteColore, NULL, Texte8Bit, NULL);
  */
  SDL_FreeSurface(TexteColore);
    
  BrosseRetour=Surface_en_bytefield(Texte8Bit, NULL);
  if (!BrosseRetour)
  {
    SDL_FreeSurface(TexteColore);
    SDL_FreeSurface(Texte8Bit);
    TTF_CloseFont(Fonte);
    return NULL;
  }
  if (!AntiAlias)
  {
  // Mappage des couleurs
  /*for (Indice=0; Indice < Texte8Bit->w * Texte8Bit->h; Indice++)
  {
    if ()
    *(BrosseRetour+Indice)=*(BrosseRetour+Indice) ? (*(BrosseRetour+Indice)+96-15) : 0;
  }*/
  }
  
  *Largeur=Texte8Bit->w;
  *Hauteur=Texte8Bit->h;
  SDL_FreeSurface(Texte8Bit);

  return BrosseRetour;
}
#endif

// Cr�e une brosse � partir des param�tres de texte demand�s.
// Si cela r�ussit, la fonction place les dimensions dans Largeur et Hauteur, 
// et retourne l'adresse du bloc d'octets.
byte *Rendu_Texte(const char *Chaine, int Taille, int AntiAlias, int *Largeur, int *Hauteur)
{
  #ifndef NOTTF 
    return Rendu_Texte_TTF(Chaine, Taille, AntiAlias, Largeur, Hauteur);
  #else
    return NULL;
  #endif
}

