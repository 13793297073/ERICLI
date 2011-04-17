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
#ifndef MODEL_H
#define MODEL_H

#include <math.h>

#include <giomm/file.h>

#include "stl.h"
#include "rfo.h"
#include "types.h"
#include "gcode.h"
#include "settings.h"

#ifdef WIN32
#  pragma warning( disable : 4244 4267)
#endif

class Progress {
 public:
  // Progress reporting
  sigc::signal< void, const char *, double > m_signal_progress_start;
  sigc::signal< void, double >               m_signal_progress_update;
  sigc::signal< void, const char * >         m_signal_progress_stop;

  // helpers
  void start (const char *label, double max)
  {
    m_signal_progress_start.emit (label, max);
  }
  void stop (const char *label)
  {
    m_signal_progress_stop.emit (label);
  }
  void update (double value)
  {
    m_signal_progress_update.emit (value);
  }
};

class Model
{
	friend class PrintInhibitor;

	bool m_printing;
	unsigned long m_unconfirmed_lines;

  // Callbacks
  static void handle_reply(rr_dev device, void *data, rr_reply reply, float value);
  static void handle_send(rr_dev device, void *cbdata, void *blkdata, const char *block, size_t len);
  static void handle_recv(rr_dev device, void *data, const char *reply, size_t len);
  bool handle_dev_fd(Glib::IOCondition cond);
  static void handle_want_writable(rr_dev device, void *data, char state);

	sigc::signal< void > m_signal_rfo_changed;

	bool m_inhibit_print;
	sigc::signal< void > m_signal_inhibit_changed;

	double m_temps[TEMP_LAST];

	SerialState m_serial_state;

public:
	SerialState get_serial_state () { return m_serial_state; }
	void serial_try_connect (bool connect);
	sigc::signal< void, SerialState > m_signal_serial_state_changed;

	double get_temp (TempType t) { return m_temps[(int)t]; }
	sigc::signal< void > m_signal_temp_changed;

	sigc::signal< void, Gtk::MessageType, const char *, const char * > m_signal_alert;
	void alert (const char *message);
	void error (const char *message, const char *secondary);

	Progress m_progress;

	// Something in the rfo changed
	sigc::signal< void > signal_rfo_changed() { return m_signal_rfo_changed; }

	sigc::signal< void > signal_inhibit_changed() { return m_signal_inhibit_changed; }
	bool get_inhibit_print() { return m_inhibit_print; }

	Model();
	~Model();
	void progess_bar_start (const char *label, double max);

	bool IsPrinting() { return m_printing; }
	void SimpleAdvancedToggle();
	void SaveConfig(Glib::RefPtr<Gio::File> file);
	void LoadConfig() { LoadConfig(Gio::File::create_for_path("repsnapper.conf")); }
	void LoadConfig(Glib::RefPtr<Gio::File> file);

	// RFO Functions
	void ReadRFO(std::string filename);

	// STL Functions
	void ReadStl(Glib::RefPtr<Gio::File> file);
	RFO_File *AddStl(RFO_Object *parent, STL stl, string filename);
	sigc::signal< void, Gtk::TreePath & > m_signal_stl_added;

	void OptimizeRotation(RFO_File *file, RFO_Object *object);
	void ScaleObject(RFO_File *file, RFO_Object *object, double scale);
	void RotateObject(RFO_File *file, RFO_Object *object, Vector4f rotate);
	bool updateStatusBar(GdkEventCrossing *event, Glib::ustring = "");

	// GCode Functions
	void init();
	void ReadGCode(Glib::RefPtr<Gio::File> file);
	void ConvertToGCode();
	void MakeRaft(float &z);
	void WriteGCode(Glib::RefPtr<Gio::File> file);

	// Communication
	bool IsConnected();
	void SimplePrint();

	void Print();
	void Continue();
	void Restart();

	void RunExtruder(double extruder_speed, double extruder_length, bool reverse);
	void SendNow(string str);
	void setPort(string s);
	void setSerialSpeed(int s );
	void SetValidateConnection(bool validate);

	void EnableTempReading(bool on);
	void SetLogFileClear(bool on);
	void SwitchPower(bool on);

	void Home(string axis);
	void Move(string axis, float distance);
	void Goto(string axis, float position);
	void STOP();

	Matrix4f &SelectedNodeMatrix(uint objectNr = 1);
	void SelectedNodeMatrices(vector<Matrix4f *> &result );
	void newObject();

	rr_dev m_device;
	sigc::connection m_devconn;

	/*- Custom button interface -*/
	void SendCustomButton(int nr);
	void SaveCustomButton();
	void TestCustomButton();
	void GetCustomButtonText(int nr);
	void RefreshCustomButtonLabels();

	void PrintButton();
	void ContinuePauseButton();
	void ClearLogs();

	Settings settings;

	// Model derived: Bounding box info
	Vector3f Center;
	Vector3f Min;
	Vector3f Max;
	vmml::Vector3f printOffset; // margin + raft

	void CalcBoundingBoxAndCenter();

	sigc::signal< void > m_model_changed;
	void ModelChanged();

	// Truly the model
	RFO rfo;
	GCode gcode;
	Glib::RefPtr<Gtk::TextBuffer> commlog, errlog, echolog;
};

#endif // MODEL_H
