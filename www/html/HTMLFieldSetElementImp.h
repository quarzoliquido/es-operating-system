// Generated by esidl (r1745).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_HTMLFIELDSETELEMENTIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_HTMLFIELDSETELEMENTIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/HTMLFieldSetElement.h>
#include "HTMLElementImp.h"

#include <org/w3c/dom/html/HTMLElement.h>
#include <org/w3c/dom/html/HTMLFormControlsCollection.h>
#include <org/w3c/dom/html/HTMLFormElement.h>
#include <org/w3c/dom/html/ValidityState.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class HTMLFieldSetElementImp : public ObjectMixin<HTMLFieldSetElementImp, HTMLElementImp>
{
public:
    // HTMLFieldSetElement
    bool getDisabled() __attribute__((weak));
    void setDisabled(bool disabled) __attribute__((weak));
    html::HTMLFormElement getForm() __attribute__((weak));
    std::u16string getName() __attribute__((weak));
    void setName(std::u16string name) __attribute__((weak));
    std::u16string getType() __attribute__((weak));
    html::HTMLFormControlsCollection getElements() __attribute__((weak));
    bool getWillValidate() __attribute__((weak));
    html::ValidityState getValidity() __attribute__((weak));
    std::u16string getValidationMessage() __attribute__((weak));
    bool checkValidity() __attribute__((weak));
    void setCustomValidity(std::u16string error) __attribute__((weak));
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::HTMLFieldSetElement::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::HTMLFieldSetElement::getMetaData();
    }
    HTMLFieldSetElementImp(DocumentImp* ownerDocument) :
        ObjectMixin(ownerDocument, u"fieldset") {
    }
    HTMLFieldSetElementImp(HTMLFieldSetElementImp* org, bool deep) :
        ObjectMixin(org, deep) {
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_HTMLFIELDSETELEMENTIMP_H_INCLUDED
