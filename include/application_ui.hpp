#ifndef APPLICATION_UI_HPP
#define APPLICATION_UI_HPP

#include <gtkmm/applicationwindow.h>
#include <gtkmm/builder.h>
#include <map>
#include <tuple>
#include "application.hpp"
#include "fr_plot.hpp"

using tuplemap = std::map<std::string, std::tuple<double, double, double>>;

class ApplicationUI : public Gtk::ApplicationWindow {
public:
    ApplicationUI(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Application* application);
    virtual ~ApplicationUI();

    static ApplicationUI* create(Application* app);

private:
    Application* app;

    tuplemap colors;

    FrequencyResponsePlot* fr_plot;

    std::vector<sigc::connection> connections;
};

#endif // APPLICATION_UI_HPP