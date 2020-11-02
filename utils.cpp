#include "utils.h"

QString Utils::readOuterXML(QXmlStreamReader *reader)
{
    auto output = QString();

    if(reader->hasError() || reader->atEnd() || !reader->isStartElement()) return output;

    auto elementName = reader->name();

    output += "<" + elementName;

    auto attributes = reader->attributes();

    for(int i = 0, c = attributes.count(); i < c; ++i)
    {
        auto attribute = attributes[i];

        output += " " + attribute.name() + "=\"" + attribute.value() + "\"";
    }

    output += ">";

    auto hasContent = false;

    while(!reader->atEnd())
    {
        auto tokenType = reader->readNext();

        if(reader->hasError()) break;

        auto leaveLoop = false;

        switch (tokenType) {
        case QXmlStreamReader::Characters:
            output += reader->text().toString().toHtmlEscaped();
            hasContent = true;
            break;

        case QXmlStreamReader::StartElement:
            output += readOuterXML(reader);
            hasContent = true;
            break;

        case QXmlStreamReader::EndElement:
            leaveLoop = true;
            break;
        default:
            break;
        }

        if(leaveLoop) break;
    }

    if(hasContent) output += "</" + elementName + ">";
    else output = output.replace(output.length() - 1, 1, "/>");

    return output;
}
