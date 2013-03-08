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
#ifndef RENDER_H
#define RENDER_H

#include "arcball.h"
#include <gtkglmm.h>

class View;
class Model;
class gllight;
class Settings;

class Render : public Gtk::GL::DrawingArea
{
  ArcBall  *m_arcBall;
  Matrix4fT m_transform;
  Vector2f  m_downPoint;
  Vector2f  m_dragStart;
  View *m_view;
  Model *get_model() const;
  Glib::RefPtr<Gtk::TreeSelection> m_selection;

  // font rendering:
  static GLuint fontlistbase;
  static int fontheight;

  float m_zoom;
  gllight *m_lights[4];

  void SetEnableLight(unsigned int lightNr, bool on);
  void CenterView();
  void selection_changed();
  guint find_object_at(gdouble x, gdouble y);
  Vector3d mouse_on_plane(double x, double y, double plane_z=0) const;

 public:
  Render (View *view, Glib::RefPtr<Gtk::TreeSelection> selection);
  ~Render();

  GtkWidget *get_widget();
  void set_model (Model *model);
  void set_zoom (float zoom) {m_zoom=zoom;};
  void zoom_to_model();
  void set_transform(const Matrix4fT &transform) {m_transform=transform;};

  static void draw_string(const Vector3d &pos, const string s);

  virtual void on_realize();
  virtual bool on_configure_event(GdkEventConfigure* event);
  virtual bool on_expose_event(GdkEventExpose* event);
  virtual bool on_motion_notify_event(GdkEventMotion* event);
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_button_release_event(GdkEventButton* event);
  virtual bool on_scroll_event(GdkEventScroll* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
};

#endif // RENDER_H
