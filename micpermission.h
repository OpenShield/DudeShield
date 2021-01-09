#ifndef __MicPermission_h_
#define __MicPermission_h_

#include <QObject>
#include <QString>

class MicPermission : public QObject
{
    Q_OBJECT
public:
    static int check_permission();
};

#endif
