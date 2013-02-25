/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <giomm/file.h>
#include <glibmm/keyfile.h>

#include "stdafx.h"


// Allow passing as a pointer to something to
// avoid including glibmm in every header.
typedef Glib::RefPtr<Gtk::Builder> Builder;


class Settings {
  bool m_user_changed;
  bool inhibit_callback; // don't update settings from gui while setting to gui

 public:

  std::string Name;
  std::string Image;
  std::string Filename;

  struct RaftSettings {
    bool Enable;
    float Size;
    struct PhasePropertiesType {
      guint  LayerCount;
      float MaterialDistanceRatio;
      float Rotation;
      float RotationPrLayer;
      float Distance;
      float Thickness;
      float Temperature;
    };
    enum PhaseType { PHASE_BASE, PHASE_INTERFACE };
    PhasePropertiesType Phase[2];
  };
  RaftSettings Raft;

  struct PrinterSettings {
    float ExtrudeAmount;
    float ExtrudeSpeed;
    float NozzleTemp;
    float BedTemp;
    int FanVoltage;
    bool Logging;
    bool ClearLogOnPrintStart;
  };
  PrinterSettings Printer;


  struct HardwareSettings {
    float MinMoveSpeedXY;
    float MaxMoveSpeedXY;
    float MinMoveSpeedZ;
    float MaxMoveSpeedZ;

    float DistanceToReachFullSpeed;

    vmml::vec3d Volume;      // Print volume
    vmml::vec3d PrintMargin;

    std::string PortName;
    int SerialSpeed;
    int KeepLines;

    bool SpeedAlways;
  };
  HardwareSettings Hardware;

  vmml::vec3d getPrintVolume() const;
  vmml::vec3d getPrintMargin() const;

  struct ExtruderSettings {
    string name;

    string GCLetter;

    float OffsetX;
    float OffsetY;
    bool  CalibrateInput; // treat 'mm' as filament input mm not of nozzle output.
    float EMaxSpeed;
    float MaxLineSpeed;
    float MaxShellSpeed;
    float ExtrusionFactor;
    float FilamentDiameter;
    float ExtrudedMaterialWidthRatio; // ratio of extruded width to (layer) height
    double GetExtrudedMaterialWidth(const double layerheight) const;
    float MinimumLineWidth;
    float MaximumLineWidth;
    /* float DownstreamMultiplier; */
    /* float DownstreamExtrusionMultiplier; */
    static double RoundedLinewidthCorrection(double extr_width, double layerheight);
    double GetExtrusionPerMM(double layerheight) const;

    bool  EnableAntiooze;
    float AntioozeDistance;
    float AntioozeAmount;
    float AntioozeSpeed;
    //float AntioozeHaltRatio;
    float AntioozeZlift;
    bool ZliftAlways;
    Vector4f DisplayColour;

    bool UseForSupport;
  };
  ExtruderSettings Extruder; // to exchange settings with GUI
  std::vector<ExtruderSettings> Extruders; // all Extruders
  std::vector<char> get_extruder_letters() const;
  Vector3d get_extruder_offset(uint num) const;
  uint GetSupportExtruder() const;
  uint selectedExtruder;
  void CopyExtruder(uint num);
  void RemoveExtruder(uint num);
  void SelectExtruder(uint num, Builder *builder=NULL);

  struct SlicingSettings {
    bool  RelativeEcode;
    bool UseTCommand;
    float LayerThickness;

    bool  UseArcs;
    float ArcsMaxAngle;
    float MinArcLength;
    bool  RoundCorners;
    float CornerRadius;

    bool MoveNearest;

    bool NoBridges;
    float BridgeExtrusion;

    bool DoInfill;
    float InfillPercent;
    float InfillRotation;
    float InfillRotationPrLayer;
    float AltInfillPercent;
    int AltInfillLayers;
    float InfillOverlap;
    bool NoTopAndBottom;
    //int SolidLayers;
    float SolidThickness;
    bool Support;
    float SupportAngle;
    float SupportWiden;
    bool Skirt;
    bool SingleSkirt;
    float SkirtHeight;
    float SkirtDistance;
    bool FillSkirt;
    int Skins;
    bool Varslicing;
    int NormalFilltype;
    float NormalFillExtrusion;
    int FullFilltype;
    float FullFillExtrusion;
    int SupportFilltype;
    float SupportExtrusion;
    float SupportInfillDistance;

    bool MakeDecor;
    int DecorFilltype;
    int DecorLayers;
    float DecorInfillDistance;
    float DecorInfillRotation;

    float MinShelltime;
    float MinLayertime;
    bool FanControl;
    int MinFanSpeed;
    int MaxFanSpeed;
    float MaxOverhangSpeed;

    guint ShellCount;
    /* bool EnableAcceleration; */
    //int ShrinkQuality;

    bool BuildSerial;
    //float SerialBuildHeight;
    bool SelectedOnly;

    float ShellOffset;
    //float Optimization;

    guint FirstLayersNum;
    float FirstLayersSpeed;
    float FirstLayersInfillDist;
    float FirstLayerHeight;
    //void GetAltInfillLayers(std::vector<int>& layers, guint layerCount) const;

    bool GCodePostprocess;
    std::string GCodePostprocessor;
  };
  SlicingSettings Slicing;

  struct MillingSettings {
    float ToolDiameter;
  };
  MillingSettings Milling;

  struct MiscSettings {
    bool SpeedsAreMMperSec;
    bool ShapeAutoplace;
    bool FileLoggingEnabled;
    bool TempReadingEnabled;
    bool ClearLogfilesWhenPrintStarts;
    int window_width;
    int window_height;
    int window_posx;
    int window_posy;
    bool ExpandLayerDisplay;
    bool ExpandModelDisplay;
    bool ExpandPAxisDisplay;
    bool SaveSingleShapeSTL;
  };
  MiscSettings Misc;

  class GCodeImpl;
  enum GCodeTextType {
    GCODE_TEXT_START,
    GCODE_TEXT_LAYER,
    GCODE_TEXT_END,
    GCODE_TEXT_TYPE_COUNT
  };
  struct GCodeType {
    GCodeImpl *m_impl;
    std::string getText(GCodeTextType t) const ;
    std::string getStartText() const { return getText (GCODE_TEXT_START); }
    std::string getLayerText() const { return getText (GCODE_TEXT_LAYER); }
    std::string getEndText()   const { return getText (GCODE_TEXT_END);   }
  };
  GCodeType GCode;

  struct DisplaySettings {
    bool DisplayGCode;
    float GCodeDrawStart;
    float GCodeDrawEnd;

    bool DisplayEndpoints;
    bool DisplayNormals;
    bool DisplayBBox;
    bool DisplayWireframe;
    bool DisplayWireframeShaded;
    bool DisplayPolygons;
    bool DisplayAllLayers;
    bool DisplayinFill;
    bool DisplayDebuginFill;
    bool DisplayDebug;
    bool DisplayLayer;
    bool DisplayFilledAreas;
    bool DisplayGCodeBorders;
    bool DisplayGCodeMoves;
    bool DisplayGCodeArrows;
    bool DisplayDebugArcs;
    bool DebugGCodeExtruders;
    bool DebugGCodeOffset;
    bool ShowLayerOverhang;
    bool DrawVertexNumbers;
    bool RandomizedLines;
    bool DrawLineNumbers;
    bool DrawOutlineNumbers;
    bool DrawCPVertexNumbers;
    bool DrawCPLineNumbers;
    bool DrawCPOutlineNumbers;
    bool DrawRulers;
    float LayerValue;
    bool LuminanceShowsSpeed;
    bool CommsDebug;
    bool TerminalProgress;
    bool PreviewLoad;


    // Rendering
    Vector4f PolygonColour;
    Vector4f WireframeColour;
    Vector4f NormalsColour;
    Vector4f EndpointsColour;
    // Vector4f GCodeExtrudeColour; // now in Extruder
    Vector4f GCodePrintingColour;
    Vector4f GCodeMoveColour;
    float    Highlight;
    float    NormalsLength;
    float    EndPointSize;
    float    TempUpdateSpeed;
  };
  DisplaySettings Display;

  // Paths we loaded / saved things to last time
  std::string STLPath;
  std::string RFOPath;
  std::string GCodePath;
  std::string SettingsPath;

  std::vector<std::string> CustomButtonGcode;
  std::vector<std::string> CustomButtonLabel;

 private:
  void set_to_gui              (Builder &builder, int i);
  void get_from_gui            (Builder &builder, int i);
  //void set_shrink_to_gui       (Builder &builder);
  //void get_shrink_from_gui     (Builder &builder);
  void set_filltypes_to_gui    (Builder &builder);
  void get_filltypes_from_gui  (Builder &builder);
  void get_port_speed_from_gui (Builder &builder);
  bool get_group_and_key       (int i, Glib::ustring &group, Glib::ustring &key);
  void get_colour_from_gui     (Builder &builder, int i);

  void set_defaults ();
 public:

  Settings();
  ~Settings();

  bool has_user_changed() const { return m_user_changed; }
  void assign_from(Settings *pSettings);

  Matrix4d getBasicTransformation(Matrix4d T) const;

  // return real mm depending on hardware extrusion width setting
  double GetInfillDistance(double layerthickness, float percent) const;

  // sync changed settings with the GUI eg. used post load
  void set_to_gui (Builder &builder, const string filter="");

  // connect settings to relevant GUI widgets
  void connect_to_ui (Builder &builder);

  void load_settings (Glib::RefPtr<Gio::File> file);
  void load_settings_as (const Glib::KeyFile &cfg,
			 const Glib::ustring onlygroup = "",
			 const Glib::ustring as_group = "");
  void save_settings (Glib::RefPtr<Gio::File> file);
  void save_settings_as (Glib::KeyFile &cfg,
			 const Glib::ustring onlygroup = "",
			 const Glib::ustring as_group = "");

  std::string get_image_path();

  sigc::signal< void > m_signal_visual_settings_changed;
  sigc::signal< void > m_signal_update_settings_gui;
  sigc::signal< void > m_signal_core_settings_changed;
};

#endif // SETTINGS_H
