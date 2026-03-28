#ifndef ENHANCEVISION_IPROGRESSPROVIDER_H
#define ENHANCEVISION_IPROGRESSPROVIDER_H

#include <QObject>
#include <QString>

namespace EnhanceVision {

class IProgressProvider
{
public:
    virtual ~IProgressProvider() = default;
    
    virtual double progress() const = 0;
    virtual QString stage() const = 0;
    virtual int estimatedRemainingSec() const = 0;
    virtual bool isValid() const = 0;
    
    virtual QObject* asQObject() = 0;
};

} // namespace EnhanceVision

Q_DECLARE_INTERFACE(EnhanceVision::IProgressProvider, 
                    "com.enhancevision.IProgressProvider/1.0")

#endif // ENHANCEVISION_IPROGRESSPROVIDER_H
