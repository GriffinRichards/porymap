#pragma once
#ifndef EVENTRAMES_H
#define EVENTRAMES_H

#include <QFrame>
#include <QLabel>

#include "eventcombobox.h"
#include "mainwindow.h"

#include "events.h"


class Project;

class EventFrame : public QFrame {
    Q_OBJECT
public:
    EventFrame(Event *event, QWidget *parent = nullptr);

    virtual void populate(Project *project);

    void invalidateValues();

protected:
    // TODO: Privatize

    QVBoxLayout *m_layoutContents = nullptr;

    bool m_populated = false;
    QPointer<Project> m_project;
    Event *m_event = nullptr;

    void setElevationEnabled(bool enabled);
    void setIcon(const QPixmap& pixmap);

    void populateDropdown(EventComboBox * combo, const QStringList &items);
    void populateScriptDropdown(EventComboBox * combo, Project * project);
    void populateMapNameDropdown(EventComboBox * combo, Project * project);
    void populateIdNameDropdown(EventComboBox * combo, Project * project, const QString &mapName, Event::Group group);

private:
    QWidget *m_widgetZ = nullptr;
    QLabel *m_icon = nullptr;
};



class ObjectFrame : public EventFrame {
    Q_OBJECT
public:
    ObjectFrame(ObjectEvent *object, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboSprite = nullptr;
    EventComboBox *m_comboMovement = nullptr;
    EventComboBox *m_comboScript = nullptr;
    EventComboBox *m_comboFlag = nullptr;
    EventComboBox *m_comboTrainerType = nullptr;
};



class CloneObjectFrame : public EventFrame {
    Q_OBJECT
public:
    CloneObjectFrame(CloneObjectEvent *clone, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboSprite = nullptr;
    EventComboBox *m_comboTargetId = nullptr;
    EventComboBox *m_comboTargetMap = nullptr;

    void tryInvalidateIdDropdown(Map *map);
};



class WarpFrame : public EventFrame {
    Q_OBJECT
public:
    WarpFrame(WarpEvent *warp, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboDestMap = nullptr;
    EventComboBox *m_comboDestWarp = nullptr;
    QPushButton *m_warning = nullptr;

    void tryInvalidateIdDropdown(Map *map);
};



class TriggerFrame : public EventFrame {
    Q_OBJECT
public:
    TriggerFrame(TriggerEvent *trigger, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboScript = nullptr;
    EventComboBox *m_comboVar = nullptr;
};



class WeatherTriggerFrame : public EventFrame {
    Q_OBJECT
public:
    WeatherTriggerFrame(WeatherTriggerEvent *weatherTrigger, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboWeather = nullptr;
};



class SignFrame : public EventFrame {
    Q_OBJECT
public:
    SignFrame(SignEvent *sign, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboFacingDir = nullptr;
    EventComboBox *m_comboScript = nullptr;
};



class HiddenItemFrame : public EventFrame {
    Q_OBJECT
public:
    HiddenItemFrame(HiddenItemEvent *hiddenItem, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    QFrame *m_quantityFrame = nullptr;
    QFrame *m_requiresItemfinderFrame = nullptr;
    EventComboBox *m_comboItem = nullptr;
    EventComboBox *m_comboFlag = nullptr;

    HiddenItemEvent *hiddenItem = nullptr;
};



class SecretBaseFrame : public EventFrame {
    Q_OBJECT
public:
    SecretBaseFrame(SecretBaseEvent *secretBase, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    EventComboBox *m_comboBaseId = nullptr;
};



class HealLocationFrame : public EventFrame {
    Q_OBJECT
public:
    HealLocationFrame(HealLocationEvent *healLocation, QWidget *parent = nullptr);

    virtual void populate(Project *project) override;

private:
    QFrame *m_respawnMapFrame = nullptr;
    QFrame *m_respawnNPCFrame = nullptr;
    EventComboBox *m_comboRespawnMap = nullptr;
    EventComboBox *m_comboRespawnNPC = nullptr;

    void tryInvalidateIdDropdown(Map *map);
};

#endif // EVENTRAMES_H
