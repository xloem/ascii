#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <2geom/path-sink.h>

auto & debug = std::cerr;

FT_Vector halfway_between( FT_Vector p1, FT_Vector p2 )
{
	FT_Vector newv;
	newv.x = p1.x + (p2.x-p1.x)/2.0;
	newv.y = p1.y + (p2.y-p1.y)/2.0;
	return newv;
}

class TTF
{
public:
	std::string filename;
	FT_Library library;
	FT_Face face;
	FT_Error error;

	TTF(std::string fname)
	{
		filename = fname;
		error = FT_Init_FreeType( &library );
		debug << "Init error code: " << error;

		// Load a typeface
		error = FT_New_Face( library, filename.c_str(), 0, &face );
		debug << "\nFace load error code: " << error;
		debug << "\nfont filename: " << filename;
		if (error) {
			std::cerr << "problem loading file " << filename << "\n";
			exit(1);
		}
		debug << "\nFamily Name: " << face->family_name;
		debug << "\nStyle Name: " << face->style_name;
		debug << "\nNumber of faces: " << face->num_faces;
		debug << "\nNumber of glyphs: " << face->num_glyphs;
	}

	~TTF()
	{
		debug << "\n<!--";
		error = FT_Done_Face( face );
		debug << "\nFree face. error code: " << error;
		error = FT_Done_FreeType( library );
		debug << "\nFree library. error code: " << error;
		debug << "\n-->\n";
	}

};


/* Draw the outline of the font as svg.
There are three main components.
1. the points
2. the 'tags' for the points
3. the contour indexes (that define which points belong to which contour)
*/
std::string do_outline(std::vector<FT_Vector> points, std::vector<char> tags, std::vector<short> contours)
{
	std::stringstream debug, svg;
	std::cout << "<!-- do outline -->\n";
	if (points.size()==0) return "<!-- font had 0 points -->";
	if (contours.size()==0) return "<!-- font had 0 contours -->";
	svg.str("");
	svg << "\n\n  <!-- draw actual outline using lines and Bezier curves-->";
	svg	<< "\n  <path fill='black' stroke='black'"
		<< " fill-opacity='0.45' "
		<< " stroke-width='2' "
		<< " d='";

	/* tag bit 1 indicates whether its a control point on a bez curve
	or not. two consecutive control points imply another point halfway
	between them */

	// Step 1. move to starting point (M x-coord y-coord )
	// Step 2. decide whether to draw a line or a bezier curve or to move
	// Step 3. for bezier: Q control-point-x control-point-y,
	//		         destination-x, destination-y
	//         for line:   L x-coord, y-coord
	//         for move:   M x-coord, y-coord

	int contour_starti = 0;
	int contour_endi = 0;
	for ( int i = 0 ; i < contours.size() ; i++ ) {
		contour_endi = contours.at(i);
		debug << "new contour starting. startpt index, endpt index:";
		debug << contour_starti << "," << contour_endi << "\n";
		int offset = contour_starti;
		int npts = contour_endi - contour_starti + 1;
		debug << "number of points in this contour: " << npts << "\n";
		debug << "moving to first pt " << points[offset].x << "," << points[offset].y << "\n";
		svg << "\n M " << points[contour_starti].x << "," << points[contour_starti].y << "\n";
		debug << "listing pts: [this pt index][isctrl] <next pt index><isctrl> [x,y] <nx,ny>\n";
		for ( int j = 0; j < npts; j++ ) {
			int thisi = j%npts + offset;
			int nexti = (j+1)%npts + offset;
			int nextnexti = (j+2)%npts + offset;
			int x = points[thisi].x;
			int y = points[thisi].y;
			int nx = points[nexti].x;
			int ny = points[nexti].y;
			int nnx = points[nextnexti].x;
			int nny = points[nextnexti].y;
			bool this_tagbit1 = (tags[ thisi ] & 1);
			bool next_tagbit1 = (tags[ nexti ] & 1);
			bool nextnext_tagbit1 = (tags[ nextnexti ] & 1);
			bool this_isctl = !this_tagbit1;
			bool next_isctl = !next_tagbit1;
			bool nextnext_isctl = !nextnext_tagbit1;
			debug << " [" << thisi << "]";
			debug << "[" << !this_tagbit1 << "]";
			debug << " <" << nexti << ">";
			debug << "<" << !next_tagbit1 << ">";
			debug << " <<" << nextnexti << ">>";
			debug << "<<" << !nextnext_tagbit1 << ">>";
			debug << " [" << x << "," << y << "]";
			debug << " <" << nx << "," << ny << ">";
			debug << " <<" << nnx << "," << nny << ">>";
			debug << "\n";

			if (this_isctl && next_isctl) {
				debug << " two adjacent ctl pts. adding point halfway between " << thisi << " and " << nexti << ":";
				debug << " reseting x and y to ";
				x = (x + nx) / 2;
				y = (y + ny) / 2;
				this_isctl = false;
				debug << " [" << x << "," << y <<"]\n";
				if (j==0) {
					debug << "first pt in contour was ctrl pt. moving to non-ctrl pt\n";
					svg << " M " << x << "," << y << "\n";
				}
			}

			if (!this_isctl && next_isctl && !nextnext_isctl) {
				svg << " Q " << nx << "," << ny << " " << nnx << "," << nny << "\n";
				debug << " bezier to " << nnx << "," << nny << " ctlx, ctly: " << nx << "," << ny << "\n";
			} else if (!this_isctl && next_isctl && nextnext_isctl) {
				debug << " two ctl pts coming. adding point halfway between " << nexti << " and " << nextnexti << ":";
				debug << " reseting nnx and nny to halfway pt";
				nnx = (nx + nnx) / 2;
				nny = (ny + nny) / 2;
				svg << " Q " << nx << "," << ny << " " << nnx << "," << nny << "\n";
				debug << " bezier to " << nnx << "," << nny << " ctlx, ctly: " << nx << "," << ny << "\n";
			} else if (!this_isctl && !next_isctl) {
				svg << " L " << nx << "," << ny << "\n";
				debug << " line to " << nx << "," << ny << "\n";			
			} else if (this_isctl && !next_isctl) {
				debug << " this is ctrl pt. skipping to " << nx << "," << ny << "\n";
			}
		}
		contour_starti = contour_endi+1;
		svg << " Z\n";
	}
	svg << "\n  '/>";
	std::cout << "\n<!--\n" << debug.str() << " \n-->\n";
	return svg.str();
}

class Glyph
{
public:
	int codepoint;
	FT_GlyphSlot slot;
	FT_Error error;
	FT_Outline ftoutline;
	FT_Glyph_Metrics gm;
	FT_Face face;
	TTF & file;

	FT_Vector* ftpoints;
	char* tags;
	short* contours;

	std::stringstream debug, tmp;
	int bbwidth, bbheight;

	Glyph( TTF &f, int codepoint )
	: file(f), codepoint(codepoint)
	{
		face = file.face;
		// Load the Glyph into the face's Glyph Slot + print details
		FT_UInt glyph_index = FT_Get_Char_Index( face, codepoint );
		debug << "<!--\n";
		// debug << "Unicode requested: " << unicode_s;
		debug << " (decimal: " << codepoint << " hex: 0x"
			<< std::hex << codepoint << std::dec << ")";
		debug << "\nGlyph index for unicode: " << glyph_index;
		error = FT_Load_Glyph( face, glyph_index, FT_LOAD_NO_SCALE );
		debug << "\nLoad Glyph into Face's glyph slot. error code: " << error;
		slot = face->glyph;
		ftoutline = slot->outline;
		char glyph_name[1024];
		FT_Get_Glyph_Name( face, glyph_index, glyph_name, 1024 );
		gm = slot->metrics;
		debug << "\nGlyph Name: " << glyph_name;
		debug << "\nGlyph Width: " << gm.width
			<< " Height: " << gm.height
			<< " Hor. Advance: " << gm.horiAdvance
			<< " Vert. Advance: " << gm.vertAdvance;

		// Print outline details, taken from the glyph in the slot.
		debug << "\nNum points: " << ftoutline.n_points;
		debug << "\nNum contours: " << ftoutline.n_contours;
		debug << "\nContour endpoint index values:";
		for ( int i = 0 ; i < ftoutline.n_contours ; i++ ) debug << " " << ftoutline.contours[i];
		debug << "\n-->\n";

		// Invert y coordinates (SVG = neg at top, TType = neg at bottom)
		ftpoints = ftoutline.points;
		for ( int i = 0 ; i < ftoutline.n_points ; i++ )
			ftpoints[i].y *= -1;

		bbheight = face->bbox.yMax - face->bbox.yMin;
		bbwidth = face->bbox.xMax - face->bbox.xMin;
		tags = ftoutline.tags;
		contours = ftoutline.contours;
		std::cout << debug.str();
	}

	std::string svgheader() {
		tmp.str("");

		tmp << "\n<svg width='" << bbwidth << "px'"
			<< " height='" << bbheight << "px'"
			<< " xmlns='http://www.w3.org/2000/svg' version='1.1'>";

		return tmp.str();
	}

	std::string svgborder()  {
		tmp.str("");
		tmp << "\n\n <!-- draw border -->";

		tmp << "\n <rect fill='none' stroke='black'"
			<< " width='" << bbwidth - 1 << "'"
			<< " height='" << bbheight - 1 << "'/>";
		return tmp.str();
	}

	std::string svgtransform() {
		// TrueType points are not in the range usually visible by SVG.
		// they often have negative numbers etc. So.. here we
		// 'transform' to make visible.
		//
		// note also that y coords of all points have been flipped during
		// init() so that SVG Y positive = Truetype Y positive
		tmp.str("");
		tmp << "\n\n <!-- make sure glyph is visible within svg window -->";
		int yadj = gm.horiBearingY + gm.vertBearingY + 100;
		int xadj = 100;
		tmp << "\n <g fill-rule='nonzero' "
			<< " transform='translate(" << xadj << " " << yadj << ")'"
			<< ">";
		return tmp.str();
	}

	std::string axes()  {
		tmp.str("");
		tmp << "\n\n  <!-- draw axes --> ";
		tmp << "\n <path stroke='blue' stroke-dasharray='5,5' d='"
			<< " M" << -bbwidth << "," << 0
			<< " L" <<  bbwidth << "," << 0
			<< " M" << 0 << "," << -bbheight
			<< " L" << 0 << "," << bbheight
			<< " '/>";
		return tmp.str();
	}

	std::string typography_box()  {
		tmp.str("");
		tmp << "\n\n  <!-- draw bearing + advance box --> ";
		int x1 = 0;
		int x2 = gm.horiAdvance;
		int y1 = -gm.vertBearingY-gm.height;
		int y2 = y1 + gm.vertAdvance;
		tmp << "\n <path stroke='blue' fill='none' stroke-dasharray='10,16' d='"
			<< " M" << x1 << "," << y1
			<< " M" << x1 << "," << y2
			<< " L" << x2 << "," << y2
			<< " L" << x2 << "," << y1
			<< " L" << x1 << "," << y1
			<< " '/>";

		return tmp.str();
	}

	std::string points()  {
		tmp.str("");
		tmp << "\n\n  <!-- draw points as circles -->";
		for ( int i = 0 ; i < ftoutline.n_points ; i++ ) {
			bool this_is_ctrl_pt = !(tags[i] & 1);
			bool next_is_ctrl_pt = !(tags[(i+1)%ftoutline.n_points] & 1);
			int x = ftpoints[i].x;
			int y = ftpoints[i].y;
			int nx = ftpoints[(i+1)%ftoutline.n_points].x;
			int ny = ftpoints[(i+1)%ftoutline.n_points].y;
			int radius = 5;
			if ( i == 0 ) radius = 10;
			std::string color;
			if (this_is_ctrl_pt) color = "none"; else color = "blue";
			if (this_is_ctrl_pt && next_is_ctrl_pt) {
				tmp << "\n  <!-- halfway pt between 2 ctrl pts -->";
				tmp << "<circle"
				  << " fill='" << "blue" << "'"
				  << " stroke='black'"
				  << " cx='" << (x+nx)/2 << "' cy='" << (y+ny)/2 << "'"
				  << " r='" << 2 << "'"
				  << "/>";
			};
			tmp << "\n  <!--" << i << "-->";
			tmp << "<circle"
				<< " fill='" << color << "'"
				<< " stroke='black'"
				<< " cx='" << ftpoints[i].x << "' cy='" << ftpoints[i].y << "'"
				<< " r='" << radius << "'"
				<< "/>";
		}

		return tmp.str();
	}

	std::string pointlines()  {
		tmp.str("");
		tmp << "\n\n  <!-- draw straight lines between points -->";
		tmp << "\n  <path fill='none' stroke='green' d='";
		tmp << "\n   M " << ftpoints[0].x << "," << ftpoints[0].y << "\n";
		tmp << "\n  '/>";
		for ( int i = 0 ; i < ftoutline.n_points-1 ; i++ ) {
			std::string dash_mod("");
			for (int j = 0 ; j < ftoutline.n_contours; j++ ) {
				if (i==contours[j])
					dash_mod = " stroke-dasharray='3'";
			}
			tmp << "\n  <path fill='none' stroke='green'";
			tmp << dash_mod;
			tmp << " d='";
 			tmp << " M " << ftpoints[i].x << "," << ftpoints[i].y;
 			tmp << " L " << ftpoints[(i+1)%ftoutline.n_points].x << "," << ftpoints[(i+1)%ftoutline.n_points].y;
			tmp << "\n  '/>";
		}
		return tmp.str();
	}

	std::string labelpts() {
		tmp.str("");
		for ( int i = 0 ; i < ftoutline.n_points ; i++ ) {
			tmp << "\n <g font-family='SVGFreeSansASCII,sans-serif' font-size='10'>\n";
			tmp << "  <text id='revision'";
			tmp << " x='" << ftpoints[i].x + 5 << "'";
			tmp << " y='" << ftpoints[i].y - 5 << "'";
			tmp << " stroke='none' fill='darkgreen'>\n";
			tmp << "  " << ftpoints[i].x  << "," << ftpoints[i].y;
			tmp << "  </text>\n";
			tmp << " </g>\n";
		}
		return tmp.str();
	}

	std::string outline()  {
		std::vector<FT_Vector> pointsv(ftpoints,ftpoints+ftoutline.n_points);
		std::vector<char> tagsv(tags,tags+ftoutline.n_points);
		std::vector<short> contoursv(contours,contours+ftoutline.n_contours);
		return do_outline(pointsv, tagsv, contoursv);
	}

	std::string svgfooter()  {
		tmp.str("");
		tmp << "\n </g>\n</svg>\n";
		return tmp.str();
	}
};
