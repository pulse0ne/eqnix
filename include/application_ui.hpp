#ifndef APPLICATION_UI_HPP
#define APPLICATION_UI_HPP

#include <gtkmm/applicationwindow.h>
#include <gtkmm/builder.h>
#include <map>
#include "application.hpp"
#include "fr_plot.hpp"
#include "logger.hpp"

class ApplicationUI : public Gtk::ApplicationWindow {
public:
    ApplicationUI(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Application* application);
    virtual ~ApplicationUI();

    static ApplicationUI* create(Application* app);

private:
    Application* app;
    std::map<std::string, Gdk::RGBA> colors;
    FrequencyResponsePlot* fr_plot;
    logging::EqnixLogger logger = logging::EqnixLogger::create("ApplicationUI");
};

#endif // APPLICATION_UI_HPP