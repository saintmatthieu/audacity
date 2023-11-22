#include <vamp-sdk/PluginAdapter.h>

#include "MyVampPlugin.h"

static Vamp::PluginAdapter<MyVampPlugin> myVampPlugin;

const VampPluginDescriptor*
vampGetPluginDescriptor(unsigned int vampApiVersion, unsigned int index)
{
   if (vampApiVersion < 1)
      return 0;

   switch (index)
   {
   case 0:
      return myVampPlugin.getDescriptor();
   default:
      return 0;
   }
}
