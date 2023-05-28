// prefs.h

#include <QStringList>

HRESULT get_default_device(IMMDevice **ppMMDevice);
HRESULT list_devices(QStringList& names);
HRESULT get_specific_device(LPCWSTR szLongName, IMMDevice **ppMMDevice);
