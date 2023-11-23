#include <vamp-sdk/PluginAdapter.h>

#include "BeatSynchronousChromagram.h"

static Vamp::PluginAdapter<BeatSynchronousChromagram> myVampPlugin;

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
