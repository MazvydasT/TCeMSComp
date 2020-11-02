#ifndef UTILS_H
#define UTILS_H

#include <QXmlStreamReader>

class Utils
{
public:
    static QString readOuterXML(QXmlStreamReader *reader);
};

#endif // UTILS_H
