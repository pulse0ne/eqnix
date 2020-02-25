// #include <gtkmm.h>
#include "logger.hpp"
#include "application_ui.hpp"

// Gtk::ApplicationWindow* window = nullptr;

// sigc::connection connection;

// static void on_button_clicked() {
//     if (window) {
//         window->hide();
//     }
// }

// int main2(int argc, char* argv[]) {
//     auto app = Gtk::Application::create(argc, argv, "com.github.pulse0ne.eqnix");

//     auto refBuilder = Gtk::Builder::create();

//     try {
//         refBuilder->add_from_resource("/com/github/pulse0ne/eqnix/application.glade");
//     } catch (const Gio::ResourceError& ex) {
//         util::warning("ResourceError: " + ex.what());
//         return 1;
//     }

//     Gtk::DrawingArea* da;
//     refBuilder->get_widget("drawing_area", da);
//     if (da) {
//         da->add_events(Gdk::ENTER_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
//         da->signal_enter_notify_event().connect([=](GdkEventCrossing* event) -> bool {
//             util::warning("enter");
//             return false;
//         });
//         da->signal_button_press_event().connect([=](GdkEventButton* btn) -> bool {
//             util::warning("clicked");
//             connection = da->signal_motion_notify_event().connect([](GdkEventMotion* motion) -> bool {
//                 util::warning("motion");
//                 return false;
//             });
//             return false;
//         });
//         da->signal_button_release_event().connect([=](GdkEventButton* btn) -> bool {
//             util::warning("release");
//             if (connection) {
//                 connection.disconnect();
//             }
//             return false;
//         });

//     }

//     refBuilder->get_widget("top_window", window);

//     if (window) {
//         Gtk::Button* pButton = nullptr;
//         refBuilder->get_widget("quit_button", pButton);
//         if (pButton) {
//             pButton->signal_clicked().connect(sigc::ptr_fun(on_button_clicked));
//         }
//         app->run(*window);
//     }

//     delete window;
//     return 0;
// }

int main(int argc, char* argv[]) {
    auto app = Application::create();
    logging::defaultLogger.debug("in main after create, before run");
    return app->run(argc, argv);
}