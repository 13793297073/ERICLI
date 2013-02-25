/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "stdafx.h"
#include "math.h"

#include "gcode.h"

#include <iostream>
#include <sstream>

#include "model.h"
#include "ui/progress.h"
#include "geometry.h"
#include "ctype.h"
#include "settings.h"
#include "render.h"


GCode::GCode()
  : gl_List(-1)
{
  Min.set(99999999.0,99999999.0,99999999.0);
  Max.set(-99999999.0,-99999999.0,-99999999.0);
  Center.set(0,0,0);
  buffer = Gtk::TextBuffer::create();
}


void GCode::clear()
{
  buffer->erase (buffer->begin(), buffer->end());
  commands.clear();
  layerchanges.clear();
  buffer_zpos_lines.clear();
  Min   = Vector3d::ZERO;
  Max   = Vector3d::ZERO;
  Center= Vector3d::ZERO;
  if (gl_List>=0)
    glDeleteLists(gl_List,1);
  gl_List = -1;
}


double GCode::GetTotalExtruded(bool relativeEcode) const
{
  if (commands.size()==0) return 0;
  if (relativeEcode) {
    double E=0;
    for (uint i=0; i<commands.size(); i++)
      E += commands[i].e;
    return E;
  } else {
    for (uint i=commands.size()-1; i>0; i--)
      if (commands[i].e>0)
	return commands[i].e;
  }
  return 0;
}

void GCode::translate(Vector3d trans)
{
  for (uint i=0; i<commands.size(); i++) {
    commands[i].where += trans;
  }
  Min+=trans;
  Max+=trans;
  Center+=trans;
}


double GCode::GetTimeEstimation() const
{
  Vector3d where(0,0,0);
  double time = 0, feedrate=0, distance=0;
  for (uint i=0; i<commands.size(); i++)
	{
	  if(commands[i].f!=0)
		feedrate = commands[i].f;
	  if (feedrate!=0) {
	    distance = (commands[i].where - where).length();
	    time += distance/feedrate*60.;
	  }
	  where = commands[i].where ;
	}
  return time;
}

string getLineAt(const Glib::RefPtr<Gtk::TextBuffer> buffer, int lineno)
{
  Gtk::TextBuffer::iterator from ,to;
  from = buffer->get_iter_at_line (lineno);
  to   = buffer->get_iter_at_line (lineno+1);
  return buffer->get_text (from, to);
}

void GCode::updateWhereAtCursor(const vector<char> &E_letters)
{
  int line = buffer->get_insert()->get_iter().get_line();
  // Glib::RefPtr<Gtk::TextBuffer> buf = iter.get_buffer();
  if (line == 0) return;
  string text = getLineAt(buffer, line-1);
  Command commandbefore(text, Vector3d::ZERO, E_letters);
  Vector3d where = commandbefore.where;
  // complete position of previous line
  int l = line;
  while (l>0 && where.x()==0) {
    l--;
    text = getLineAt(buffer, l);
    where.x() = Command(text, Vector3d::ZERO, E_letters).where.x();
  }
  l = line;
  while (l>0 && where.y()==0) {
    l--;
    text = getLineAt(buffer, l);
    where.y() = Command(text, Vector3d::ZERO, E_letters).where.y();
  }
  l = line;
  // find last z pos fast
  if (buffer_zpos_lines.size()>0)
    for (uint i = buffer_zpos_lines.size()-1; i>0 ;i--)
      {
	if (int(buffer_zpos_lines[i]) <= l) {
	  text = getLineAt(buffer, buffer_zpos_lines[i]);
	  //cerr << text << endl;
	  Command c(text, Vector3d::ZERO, E_letters);
	  where.z() = c.where.z();
	  if (where.z()!=0) break;
	}
      }
  while (l>0 && where.z()==0) {
    l--;
    text = getLineAt(buffer, l);
    Command c(text, Vector3d::ZERO, E_letters);
    where.z() = c.where.z();
  }
  // current move:
  text = getLineAt(buffer, line);
  Command command(text, where, E_letters);
  Vector3d dwhere = command.where - where;
  where.z() -= 0.0000001;
  currentCursorWhere = where+dwhere;
  currentCursorCommand = command;
  currentCursorFrom = where;
}


void GCode::Read(Model *model, const vector<char> E_letters,
		 ViewProgress *progress, string filename)
{
	clear();

	ifstream file;
	file.open(filename.c_str());		//open a file
	file.seekg (0, ios::end);
	double filesize = double(file.tellg());
	file.seekg (0);

	progress->start(_("Loading GCode"), filesize);
	int progress_steps=(int)(filesize/1000);
	if (progress_steps==0) progress_steps=1;

	buffer_zpos_lines.clear();

	if(!file.good())
	{
//		MessageBrowser->add(str(boost::format("Error opening file %s") % Filename).c_str());
		return;
	}

	set_locales("C");

	uint LineNr = 0;

	string s;

	bool relativePos = false;
	Vector3d globalPos(0,0,0);
	Min.set(99999999.0,99999999.0,99999999.0);
	Max.set(-99999999.0,-99999999.0,-99999999.0);

	std::vector<Command> loaded_commands;

	double lastZ=0.;
	double lastE=0.;
	double lastF=0.;
	layerchanges.clear();

	stringstream alltext;

	int current_extruder = 0;

	while(getline(file,s))
	{
	  alltext << s << endl;

		LineNr++;
		unsigned long fpos = file.tellg();
		if (fpos%progress_steps==0) if (!progress->update(fpos)) break;

		Command command;

		if (relativePos)
		  command = Command(s, Vector3d::ZERO, E_letters);
		else
		  command = Command(s, globalPos, E_letters);

		if (command.Code == COMMENT) {
		  continue;
		}
		if (command.Code == UNKNOWN) {
		  cerr << "Unknown GCode " << s << endl;
		  continue;
		}
		if (command.Code == RELATIVEPOSITIONING) {
		  relativePos = true;
		  continue;
		}
		if (command.Code == ABSOLUTEPOSITIONING) {
		  relativePos = false;
		  continue;
		}
		if (command.Code == SELECTEXTRUDER) {
		  current_extruder = command.extruder_no;
		  continue;
		}
		command.extruder_no = current_extruder;

		// not used yet
		// if (command.Code == ABSOLUTE_ECODE) {
		//   relativeE = false;
		//   continue;
		// }
		// if (command.Code == RELATIVE_ECODE) {
		//   relativeE = true;
		//   continue;
		// }

		if (command.e == 0)
		  command.e  = lastE;
		else
		  lastE = command.e;

		if (command.f != 0)
		  lastF = command.f;
		else
		  command.f = lastF;

		// cout << s << endl;
		//cerr << command.info()<< endl;
		// if(command.where.x() < -100)
		//   continue;
		// if(command.where.y() < -100)
		//   continue;

		if (command.Code == SETCURRENTPOS) {
		  continue;//if (relativePos) globalPos = command.where;
		}
		else {
		  command.addToPosition(globalPos, relativePos);
		}

		if (globalPos.z() < 0){
		  cerr << "GCode below zero!"<< endl;
		  continue;
		}

		if ( command.Code == RAPIDMOTION ||
		     command.Code == COORDINATEDMOTION ||
		     command.Code == ARC_CW ||
		     command.Code == ARC_CCW ||
		     command.Code == GOHOME ) {

		  if(globalPos.x() < Min.x())
		    Min.x() = globalPos.x();
		  if(globalPos.y() < Min.y())
		    Min.y() = globalPos.y();
		  if(globalPos.z() < Min.z())
		    Min.z() = globalPos.z();
		  if(globalPos.x() > Max.x())
		    Max.x() = globalPos.x();
		  if(globalPos.y() > Max.y())
		    Max.y() = globalPos.y();
		  if(globalPos.z() > Max.z())
		    Max.z() = globalPos.z();
		  if (globalPos.z() > lastZ) {
		    // if (lastZ > 0){ // don't record first layer
		    unsigned long num = loaded_commands.size();
		    layerchanges.push_back(num);
		    loaded_commands.push_back(Command(LAYERCHANGE, layerchanges.size()));
		    // }
		    lastZ = globalPos.z();
		    buffer_zpos_lines.push_back(LineNr-1);
		  }
		  else if (globalPos.z() < lastZ) {
		    lastZ = globalPos.z();
		    if (layerchanges.size()>0)
		      layerchanges.erase(layerchanges.end()-1);
		  }
		}
		loaded_commands.push_back(command);
	}

	file.close();
	reset_locales();

	commands = loaded_commands;

	buffer->set_text(alltext.str());

	Center = (Max + Min)/2;

	model->m_signal_gcode_changed.emit();

	double time = GetTimeEstimation();
	int h = (int)time/3600;
	int min = ((int)time%3600)/60;
	int sec = ((int)time-3600*h-60*min);
	cerr << "GCode Time Estimation "<< h <<"h "<<min <<"m " <<sec <<"s" <<endl;
	//??? to statusbar or where else?
}

int GCode::getLayerNo(const double z) const
{
  if (layerchanges.size()>0) // have recorded layerchange indices -> draw whole layers
    for(uint i=0;i<layerchanges.size() ;i++) {
      if (commands.size() > layerchanges[i]) {
	if (commands[layerchanges[i]].where.z() >= z) {
	  //cerr  << " _ " <<  i << endl;
	  return i;
	}
      }
    }
  return -1;
}

int GCode::getLayerNo(const unsigned long commandno) const
{
  if (commandno<0) return commandno;
  if (layerchanges.size()>0) {// have recorded layerchange indices -> draw whole layers
    if (commandno > layerchanges.back()  && commandno < commands.size()) // last layer?
      return layerchanges.size()-1;
    for(uint i=0;i<layerchanges.size() ;i++)
      if (layerchanges[i] > commandno)
	return (i-1);
  }
  return -1;
}

unsigned long GCode::getLayerStart(const uint layerno) const
{
  if (layerchanges.size() > layerno) return layerchanges[layerno];
  return 0;
}
unsigned long GCode::getLayerEnd(const uint layerno) const
{
  if (layerchanges.size()>layerno+1) return layerchanges[layerno+1]-1;
  return commands.size()-1;
}
void GCode::draw(const Settings &settings, int layer,
		 bool liveprinting, int linewidth)
{
	/*--------------- Drawing -----------------*/

  //cerr << "gc draw "<<layer << " - " <<liveprinting << endl;

	Vector3d thisPos(0,0,0);
	uint start = 0, end = 0;
        uint n_cmds = commands.size();
	bool arrows = true;

	if (layerchanges.size()>0) {
            // have recorded layerchange indices -> draw whole layers
	    if (layer>-1) {
	      if (layer != 0)
		start = layerchanges[layer];

	      if (layer < (int)layerchanges.size()-1)
		end = layerchanges[layer+1];
	      else
		end = commands.size();
	    }
	    else {
              int n_changes = layerchanges.size();
	      int sind = 0;
	      int eind = 0;

              if (n_changes > 0) {
                sind = (uint)ceil(settings.Display.GCodeDrawStart*(n_changes-1)/Max.z());
	        eind = (uint)ceil(settings.Display.GCodeDrawEnd  *(n_changes-1)/Max.z());
              }
	      if (sind>=eind) {
		eind = MIN(sind+1, n_changes-1);
              }
	      else arrows = false; // arrows only for single layers

              sind = CLAMP(sind, 0, n_changes-1);
              eind = CLAMP(eind, 0, n_changes-1);

	      if (sind == 0) start = 0;
	      else  start = layerchanges[sind];
	      //if (start>0) start-=1; // get one command before layer
	      end = layerchanges[eind];
	      if (sind == n_changes-1) end = commands.size(); // get last layer
	      if (eind == n_changes-1) end = commands.size(); // get last layer
	    }
	}
	else {
          if (n_cmds > 0) {
	    start = (uint)(settings.Display.GCodeDrawStart*(n_cmds)/Max.z());
	    end =   (uint)(settings.Display.GCodeDrawEnd  *(n_cmds)/Max.z());
          }
	}

	drawCommands(settings, start, end, liveprinting, linewidth,
		     arrows && settings.Display.DisplayGCodeArrows,
		     !liveprinting && settings.Display.DisplayGCodeBorders);

	if (currentCursorWhere!=Vector3d::ZERO) {
	  glDisable(GL_DEPTH_TEST);
	  // glPointSize(10);
	  // glLineWidth(5);
	  // glColor4f(1.f,0.f,1.f,1.f);
	  // glBegin(GL_POINTS);
	  // glVertex3dv(currentCursorWhere);
	  // glEnd();
	  // glBegin(GL_LINES);
	  // glVertex3dv(currentCursorFrom);
	  // glVertex3dv(currentCursorWhere);
	  // glEnd();
	  const Vector3d offset =
	    Vector3d(settings.Extruders[currentCursorCommand.extruder_no].OffsetX,
		     settings.Extruders[currentCursorCommand.extruder_no].OffsetY, 0.);
	  currentCursorCommand.draw(currentCursorFrom, offset, 7,
				    Vector4f(1.f,0.f,1.f,1.f),
				    0., true, false);
	}
}


void GCode::drawCommands(const Settings &settings, uint start, uint end,
			 bool liveprinting, int linewidth, bool arrows, bool boundary)
{
  // if (gl_List < 0) {
  //   gl_List = glGenLists(1);
  //   glNewList(gl_List, GL_COMPILE);
  //   cerr << "list " << gl_List << endl;

	double LastE=0.0;
	bool extruderon = false;
	// Vector4f LastColor = Vector4f(0.0f,0.0f,0.0f,1.0f);
	Vector4f Color = Vector4f(0.0f,0.0f,0.0f,1.0f);

        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        uint n_cmds = commands.size();
	if (n_cmds==0) return;
	Vector3d defaultpos(0,0,0);
	Vector3d pos(0,0,0);

	bool relativeE = settings.Slicing.RelativeEcode;

	bool debug_arcs = settings.Display.DisplayDebugArcs;

	double extrusionwidth = 0;
	if (boundary)
	  extrusionwidth =
	    settings.Extruder.GetExtrudedMaterialWidth(settings.Slicing.LayerThickness);

	start = CLAMP (start, 0, n_cmds-1);
	end = CLAMP (end, 0, n_cmds-1);

	if (end<=start) return;

	// get starting point
	if (start>0) {
	  uint i = start;
	  while ((commands[i].is_value || commands[i].where == defaultpos) && i < end)
	    i++;
	  pos = commands[i].where;
        }

	// draw begin
	glPointSize(20);
	glBegin(GL_POINTS);
	//glColor4f(1.,0.1,0.1,ccol[3]);
	glVertex3dv((GLdouble*)&pos);
	glEnd();

	Vector3d last_extruder_offset = Vector3d::ZERO;

	(void) extruderon; // calm warnings
	for(uint i=start; i <= end; i++)
	{
	        Vector3d extruder_offset = Vector3d::ZERO;
	        //Vector3d next_extruder_offset = Vector3d::ZERO;

		// TO BE FIXED:
		if (!settings.Display.DebugGCodeOffset) { // show all together
		  extruder_offset = settings.get_extruder_offset(commands[i].extruder_no);
		  pos -= extruder_offset - last_extruder_offset;
		  last_extruder_offset = extruder_offset;
		}
		double extrwidth = extrusionwidth;
	        if (commands[i].is_value) continue;
		switch(commands[i].Code)
		{
		case SETSPEED:
		case ZMOVE:
		case EXTRUDERON:
			extruderon = true;
			break;
		case EXTRUDEROFF:
			extruderon = false;
			break;
		// case COORDINATEDMOTION3D: // old 3D gcode
		//   if (extruderon) {
		//     if (liveprinting) {
		//       Color = settings.Display.GCodePrintingColour;
		//     } else
		//       Color = settings.Display.GCodeExtrudeColour;
		//   }
		//   else {
		//     Color = settings.Display.GCodeMoveColour;
		//     extrwidth = 0;
		//   }
		//   commands[i].draw(pos, linewidth, Color, extrwidth,
		// 		   arrows, debug_arcs);
		//   LastE=commands[i].e;
		//   break;
		case ARC_CW:
		case ARC_CCW:
		  if (i==start) {
		    break; // don't draw arcs at beginning (wrong startpoint)
		  }
		case COORDINATEDMOTION:
		  {
		    double speed = commands[i].f;
		    double luma = 1.;
		    if( (!relativeE && commands[i].e == LastE)
			|| (relativeE && commands[i].e == 0) ) // move only
		      {
			if (settings.Display.DisplayGCodeMoves) {
			  luma = 0.3 + 0.7 * speed / settings.Hardware.MaxMoveSpeedXY / 60;
			  Color = settings.Display.GCodeMoveColour;
			  extrwidth = 0;
			} else {
			   pos = commands[i].where;
			   break;
			}
		      }
		    else
		      {
			luma = 0.3 + 0.7 * speed / settings.Extruder.MaxLineSpeed / 60;
			if (liveprinting) {
			  Color = settings.Display.GCodePrintingColour;
			} else {
			  Color = settings.Extruders[commands[i].extruder_no].DisplayColour;
			}
			if (settings.Display.DebugGCodeExtruders) {
			  ostringstream o; o << commands[i].extruder_no+1;
			  Render::draw_string( (pos + commands[i].where) / 2. + extruder_offset,
					       o.str());
			}
		      }
		    if (settings.Display.LuminanceShowsSpeed)
		      Color *= luma;
		    commands[i].draw(pos, extruder_offset, linewidth,
				     Color, extrwidth, arrows, debug_arcs);
		    LastE=commands[i].e;
		    break;
		  }
		case RAPIDMOTION:
		  {
		    Color = settings.Display.GCodeMoveColour;
		    commands[i].draw(pos, extruder_offset, 1, Color,
				     extrwidth, arrows, debug_arcs);
		    break;
		  }
		default:
			break; // ignored GCodes
		}
		//if(commands[i].Code != EXTRUDERON && commands[i].Code != EXTRUDEROFF)
		//pos = commands[i].where;
	}
	glLineWidth(1);
  //   glEndList();
  // }
  // glCallList(gl_List);
}

// bool add_text_filter_nan(string str, string &GcodeTxt)
// {
//   if (int(str.find("nan"))<0)
//     GcodeTxt += str;
//   else {
//     cerr << "not appending " << str << endl;
//     //cerr << "find: " << str.find("nan") << endl;
//     return false;
//   }
//   return true;
// }



void GCode::MakeText(string &GcodeTxt,
		     const Settings &settings,
		     ViewProgress * progress)
{
  string GcodeStart = settings.GCode.getStartText();
  string GcodeLayer = settings.GCode.getLayerText();
  string GcodeEnd   = settings.GCode.getEndText();

	double lastE = -10;
	double lastF = 0; // last Feedrate (can be omitted when same)
	Vector3d pos(0,0,0);
	Vector3d LastPos(-10,-10,-10);
	std::stringstream oss;

	Glib::Date date;
	date.set_time_current();
	Glib::TimeVal time;
	time.assign_current_time();
	GcodeTxt += "; GCode by Repsnapper, "+
	  date.format_string("%a, %x") +
	  //time.as_iso8601() +
	  "\n";

	GcodeTxt += "\n; Startcode\n"+GcodeStart + "; End Startcode\n\n";

	layerchanges.clear();
	if (progress) progress->restart(_("Collecting GCode"), commands.size());
	int progress_steps=(int)(commands.size()/100);
	if (progress_steps==0) progress_steps=1;

	for (uint i = 0; i < commands.size(); i++) {
	  char E_letter;
	  if (settings.Slicing.UseTCommand) // use first extruder's code for all extuders
	    E_letter = settings.Extruders[0].GCLetter[0];
	  else
	    E_letter = settings.Extruders[commands[i].extruder_no].GCLetter[0];
	  if (progress && i%progress_steps==0 && !progress->update(i)) break;

	  if ( commands[i].Code == LAYERCHANGE ) {
	    layerchanges.push_back(i);
	    if (GcodeLayer.length()>0)
	      GcodeTxt += "\n; Layerchange GCode\n" + GcodeLayer +
		"; End Layerchange GCode\n\n";
	  }

	  if ( commands[i].where.z() < 0 )  {
	    cerr << i << " Z < 0 "  << commands[i].info() << endl;
	  }
	  else {
	    GcodeTxt += commands[i].GetGCodeText(LastPos, lastE, lastF,
						 settings.Slicing.RelativeEcode,
						 E_letter,
						 settings.Hardware.SpeedAlways) + "\n";
	  }
	}

	GcodeTxt += "\n; End GCode\n" + GcodeEnd + "\n";

	buffer->set_text (GcodeTxt);

	// save zpos line numbers for faster finding
	buffer_zpos_lines.clear();
	uint blines = buffer->get_line_count();
	for (uint i = 0; i < blines; i++) {
	  const string line = getLineAt(buffer, i);
	  if (line.find("Z") != string::npos ||
	      line.find("z") != string::npos)
	    buffer_zpos_lines.push_back(i);
	}

	if (progress) progress->stop();

}

// void GCode::Write (Model *model, string filename)
// {
//   FILE *file;

//   file = fopen (filename.c_str(), "w+");
//   if (!file)
//     model->alert (_("failed to open file"));
//   else {
//     fprintf (file, "%s", buffer->get_text().c_str());
//     fclose (file);
//   }
// }

// bool GCode::append_text (const std::string &line)
// {
//   if (int(line.find("nan"))<0{)
//     buffer->insert (buffer->end(), line);
//     return true;
//   }
//   else {
//     cerr << "not appending line \"" << line << "\"" << endl;
//     return false;
//   }
// }


std::string GCode::get_text () const
{
  return buffer->get_text();
}



///////////////////////////////////////////////////////////////////////////////////



GCodeIter::GCodeIter (Glib::RefPtr<Gtk::TextBuffer> buffer) :
  m_buffer (buffer),
  m_it (buffer->begin()),
  m_line_count (buffer->get_line_count()),
  m_cur_line (1)
{
}

void GCodeIter::set_to_lineno(long lineno)
{
  m_cur_line = max((long)0,lineno);
  m_it = m_buffer->get_iter_at_line (m_cur_line);
}

std::string GCodeIter::next_line()
{
  Gtk::TextBuffer::iterator last = m_it;
  m_it = m_buffer->get_iter_at_line (m_cur_line++);
  return m_buffer->get_text (last, m_it);
}
std::string GCodeIter::next_line_stripped()
{
  string line = next_line();
  size_t pos = line.find(";");
  if (pos!=string::npos){
    line = line.substr(0,pos);
  }
  size_t newline = line.find("\n");
  if (newline==string::npos)
    line += "\n";
  return line;
}

bool GCodeIter::finished()
{
  return m_cur_line > m_line_count;
}

GCodeIter *GCode::get_iter ()
{
  GCodeIter *iter = new GCodeIter (buffer);
  iter->time_estimation = GetTimeEstimation();
  return iter;
}

Command GCodeIter::getCurrentCommand(Vector3d defaultwhere,
				     const vector<char> &E_letters)
{
  Gtk::TextBuffer::iterator from ,to;
  // cerr <<"currline" << defaultwhere << endl;
  // cerr <<"currline" << (int) m_cur_line << endl;
  from = m_buffer->get_iter_at_line (m_cur_line);
  to   = m_buffer->get_iter_at_line (m_cur_line+1);
  Command command(m_buffer->get_text (from, to), defaultwhere, E_letters);
  return command;
}

