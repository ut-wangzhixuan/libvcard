/**
 *
 * This file is part of the libvcard project.
 *
 * Copyright (C) 2010, Emanuele Bertoldi (Card Tech srl).
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Revision$
 * $Date$
 */

#include "vcard.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>

vCard::vCard()
{
}

vCard::vCard(const vCard& vcard)
{
   m_properties = vcard.properties();
}

vCard::vCard(const vCardPropertyList& properties)
    :   m_properties(properties)
{
}

vCard::~vCard()
{
}

void vCard::addProperty(const vCardProperty& property)
{
    int count = m_properties.count();
    for (int i = 0; i < count; i++)
    {
        vCardProperty current = m_properties.at(i);
        if (current.name() == property.name() && current.params() == property.params())
        {
            m_properties[i] = property;
            return;
        }
    }

    m_properties.append(property);

}

void vCard::addProperties(const vCardPropertyList& properties)
{
    foreach (vCardProperty property, properties)
        addProperty(property);
}

void vCard::removeProperties(const QString &name)
{
    for (int i = m_properties.count(); --i >= 0; )
    {
        vCardProperty current = m_properties.at(i);
        if (current.name() == name)
            m_properties.removeAt(i);
    }
}

vCardProperty vCard::property(const QString& name, const vCardParamList& params, bool strict) const
{
    int count = m_properties.count();
    for (int i = 0; i < count; ++i)
    {
        vCardProperty current = m_properties.at(i);
        if (current.name() == name)
        {
            vCardParamList current_params = current.params();

            if (strict)
            {
                if (params != current_params)
                    continue;
            }

            else
            {
                foreach (vCardParam param, params)
                    if (!current_params.contains(param))
                        continue;
            }

            return current;
        }
    }

    return vCardProperty();
}

const vCardProperty vCard::properties(const QString& name) const
{
  QStringList Values;

  foreach (const vCardProperty Property, m_properties)
      if (Property.name() == name)
          Values.append(Property.values());

  return !Values.isEmpty()?vCardProperty(name, Values):vCardProperty();
}

bool vCard::contains(const QString& name, const vCardParamList& params, bool strict) const
{
    int count = m_properties.count();
    for (int i = 0; i < count; i++)
    {
        vCardProperty current = m_properties.at(i);
        if (current.name() != name)
            continue;

        vCardParamList current_params = current.params();

        if (strict)
        {
            if (params != current_params)
                continue;
        }

        else
        {
            foreach (vCardParam param, params)
                if (!current_params.contains(param))
                    continue;
        }

        return true;
    }

    return false;
}

bool vCard::contains(const vCardProperty& property) const
{
    return m_properties.contains(property);
}

bool vCard::isValid() const
{
    if (m_properties.isEmpty())
        return false;

    foreach (vCardProperty prop, m_properties)
        if (!prop.isValid())
            return false;

    return true;
}

int vCard::count() const
{
    return m_properties.count();
}

QByteArray vCard::toByteArray(vCardVersion version) const
{
    QStringList lines;

    lines.append(VC_BEGIN_TOKEN);

    switch (version)
    {
        case VC_VER_2_1:
            lines.append(vCardProperty(VC_VERSION, "2.1").toByteArray());
            break;

        case VC_VER_3_0:
            lines.append(vCardProperty(VC_VERSION, "3.0").toByteArray());
            break;

        default:
            return QByteArray();
    }

    foreach (vCardProperty property, this->properties())
        lines.append(property.toByteArray(version));

    lines.append(VC_END_TOKEN);

    return lines.join(QString(VC_END_LINE_TOKEN)).toUtf8();
}

bool vCard::saveToFile(const QString& filePath) const
{
    QFileInfo fi(filePath);
    QString sPath = fi.path();
    QString sName = fi.fileName();

    QDir dir = QDir::root();
    if (dir.cd(sPath)) {

        QFile file(dir.filePath(sName));

        if (file.open(QFile::WriteOnly)) {

            file.write(this->toByteArray());

            file.close();

            return true;
        }
    }

    return false;
}

bool vCard::saveToFile(const QList<vCard> &vCardList, const QString &filepath)
{
    QFileInfo fi(filepath);
    QString sPath = fi.path();
    QString sName = fi.fileName();

    QDir dir = QDir::root();
    if (dir.cd(sPath)) {

        QFile file(dir.filePath(sName));

        if (file.open(QFile::WriteOnly)) {

            foreach (auto v, vCardList) {
                file.write(v.toByteArray() + VC_END_LINE_TOKEN);
            }

            file.close();

            return true;
        }
    }

    return false;
}

QList<vCard> vCard::fromByteArray(const QByteArray& data)
{
    QList<vCard> vcards;
    vCard current;
    bool started = false;

    QList<QByteArray> lines = data.split(VC_END_LINE_TOKEN);

    QByteArray current_line;
    for(QList<QByteArray>::const_iterator it = lines.begin();
        it != lines.end();
        ++it)
    {
        QList<QByteArray>::const_iterator next = it+1;

        if(!it->startsWith(' '))
        {
            current_line = it->simplified();
        }

        if(next != lines.end() && next->startsWith(' '))
        {
            // unfold long line according to rfc6350 "3.2.Line Delimiting and Folding"
            // " ... Unfolding is accomplished by regarding CRLF immediately
            // followed by a white space character ... "
            current_line.append(next->simplified());
        }
        else
        {
            if ((current_line == VC_BEGIN_TOKEN) && !started){
                started = true;
            }
            else if ((current_line == VC_END_TOKEN) && started)
            {
                vcards.append(current);
                current.clear();
                started = false;
            }
            else if (started)
            {
                vCardPropertyList props = vCardProperty::fromByteArray(current_line);
                current.addProperties(props);
            }

            current_line.clear();
        }
    }
    return vcards;
}

QList<vCard> vCard::fromFile(const QString& filename)
{
    QList<vCard> vcards;

    QFile input(filename);
    if (input.open(QFile::ReadOnly | QFile::Text))
    {
        QTextCodec *tc = QTextCodec::codecForName("UTF-8");

        QByteArray buff = input.readAll();

        QTextCodec::ConverterState state;
        QString tmpStr = tc->toUnicode(buff, 100, &state);

        if (state.invalidChars) {
            tmpStr = QTextCodec::codecForName("GBK")->toUnicode(buff);
        } else {
            tmpStr = tc->toUnicode(buff);
        }

        vcards = vCard::fromByteArray(tmpStr.toLocal8Bit().data());
        input.close();
    }

    return vcards;
}
