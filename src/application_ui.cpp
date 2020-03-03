#include "application_ui.hpp"
#include "util.hpp"

ApplicationUI::ApplicationUI(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Application* application)
    : Gtk::ApplicationWindow(cobject), app(application) {
    util::debug("in ApplicationUI constructor");
    colors = {
        {"background", Gdk::RGBA("rgb(33,39,44)")},
        {"accent", Gdk::RGBA("rgb(255,195,14)")},
        {"grid-lines", Gdk::RGBA("rgb(64,64,64)")},
        {"grid-lines-bright", Gdk::RGBA("rgb(92,92,92)")}
    };
    builder->get_widget_derived("drawing_area", fr_plot, colors, application->pipeline->get_equalizer());
    util::debug("initialized widget");

    set_default_size(300, 200);
}

ApplicationUI::~ApplicationUI() {}

ApplicationUI* ApplicationUI::create(Application* app) {
    util::debug("in ApplicationUI::create");
    auto builder = Gtk::Builder::create_from_resource("/com/github/pulse0ne/eqnix/application.glade");
    ApplicationUI* window = nullptr;

    builder->get_widget_derived("application_ui", window, app);
    util::debug("initialized window");

    return window;
}