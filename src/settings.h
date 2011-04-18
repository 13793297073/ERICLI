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
#include <vmmlib/vector3.h>

enum SHRINK_QUALITY { SHRINK_FAST, SHRINK_LOGICK };

class Builder;
class Settings {
 public:

  std::string Filename;

  bool   RaftEnable;
  struct RaftSettings {
    float Size;
    struct PhasePropertiesType {
      uint  LayerCount;
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

  struct HardwareSettings {
    float MinPrintSpeedXY;
    float MaxPrintSpeedXY;
    float MinPrintSpeedZ;
    float MaxPrintSpeedZ;

    float DistanceToReachFullSpeed;
    float ExtrusionFactor;

    float LayerThickness;

    vmml::Vector3f	Volume;      // Print volume
    vmml::Vector3f	PrintMargin;
    float       ExtrudedMaterialWidth;

    std::string PortName;
    int SerialSpeed;
    bool ValidateConnection;
    int KeepLines;

    int ReceivingBufferSize;

    float DownstreamMultiplier;
    float DownstreamExtrusionMultiplier;
  };
  HardwareSettings Hardware;

  struct SlicingSettings {
    bool  UseIncrementalEcode;
    bool  Use3DGcode;

    bool  EnableAntiooze;
    float AntioozeDistance;
    float AntioozeSpeed;

    float InfillDistance;
    float InfillRotation;
    float InfillRotationPrLayer;
    float AltInfillDistance;
    std::string AltInfillLayersText;

    bool ShellOnly;
    uint ShellCount;
    bool EnableAcceleration;
    int ShrinkQuality;

    float Optimization;

    void GetAltInfillLayers(std::vector<int>& layers, uint layerCount) const;
  };
  SlicingSettings Slicing;

  struct MiscSettings {
    bool FileLoggingEnabled;
    bool TempReadingEnabled;
    bool ClearLogfilesWhenPrintStarts;
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
    std::string getText(GCodeTextType t);
    std::string getStartText() { return getText (GCODE_TEXT_START); }
    std::string getLayerText() { return getText (GCODE_TEXT_LAYER); }
    std::string getEndText()   { return getText (GCODE_TEXT_END);   }
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
    bool DisplayCuttingPlane;
    bool DrawVertexNumbers;
    bool DrawLineNumbers;
    bool DrawOutlineNumbers;
    bool DrawCPVertexNumbers;
    bool DrawCPLineNumbers;
    bool DrawCPOutlineNumbers;
    float CuttingPlaneValue;
    bool LuminanceShowsSpeed;

    // Rendering
    vmml::Vector4f PolygonRGBA;
    vmml::Vector4f WireframeRGBA;
    vmml::Vector4f NormalsRGBA;
    vmml::Vector4f EndpointsRGBA;
    vmml::Vector4f GCodeExtrudeRGBA;
    vmml::Vector4f GCodeMoveRGBA;
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
  void set_shrink_to_gui       (Builder &builder);
  void get_shrink_from_gui     (Builder &builder);
  void get_port_speed_from_gui (Builder &builder);
  bool get_group_and_key       (int i, Glib::ustring &group, Glib::ustring &key);
  void get_colour_from_gui     (Builder &builder, int i);

  void set_defaults ();
 public:

  Settings ();
  ~Settings();

  // sync changed settings with the GUI eg. used post load
  void set_to_gui (Builder &builder);

  // connect settings to relevant GUI widgets
  void connect_to_ui (Builder &builder);

  void load_settings (Glib::RefPtr<Gio::File> file);
  void save_settings (Glib::RefPtr<Gio::File> file);

  sigc::signal< void > m_signal_visual_settings_changed;
  sigc::signal< void > m_signal_update_settings_gui;
};

#endif // SETTINGS_H
