/**
 * @file
 * VuoLeap interface.
 *
 * @copyright Copyright © 2012–2013 Kosada Incorporated.
 * This code may be modified and distributed under the terms of the MIT License.
 * For more information, see http://vuo.org/license.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "node.h"

/**
 * An object for listening to a Leap Motion controller.
 */
typedef void * VuoLeap;

VuoLeap VuoLeap_startListening
(
		VuoOutputTrigger(receivedFrame, VuoLeapFrame)
);
void VuoLeap_stopListening(VuoLeap *leap);

#ifdef __cplusplus
}
#endif