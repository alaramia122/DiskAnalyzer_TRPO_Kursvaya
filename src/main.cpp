#include <QApplication>
#include <QMainWindow>


int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QMainWindow window;

    window.resize(1200, 800);
    window.setWindowTitle("Disk Analyzer");

    window.show();


    return application.exec();
}