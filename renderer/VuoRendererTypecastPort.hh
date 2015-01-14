/**
 * @file
 * VuoRendererTypecastPort interface.
 *
 * @copyright Copyright © 2012–2013 Kosada Incorporated.
 * This interface description may be modified and distributed under the terms of the GNU Lesser General Public License (LGPL) version 2 or later.
 * For more information, see http://vuo.org/license.
 */

#ifndef VUORENDERERTYPECASTPORT_HH
#define VUORENDERERTYPECASTPORT_HH

#include "VuoRendererPort.hh"

/**
 * Renders a typecast port.
 */
class VuoRendererTypecastPort : public VuoRendererPort
{
public:
	VuoRendererTypecastPort(VuoRendererNode *uncollapsedTypecastNode, VuoRendererPort *replacedPort, VuoRendererSignaler *signaler);

	QRectF boundingRect(void) const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

	VuoRendererNode * getUncollapsedTypecastNode(void) const;
	VuoRendererPort * getChildPort(void) const;
	VuoRendererPort * getReplacedPort(void) const;
	qreal getChildPortXOffset(void) const;

	QPainterPath getPortPath(bool includeNormalPort, bool includeFlag, QPainterPath *outsetPath) const;

private:
	QString sourceType;
	QString destinationType;
	VuoRendererPort * childPort;
	VuoRendererPort * replacedPort;
	VuoRendererNode * uncollapsedTypecastNode;

	void setUncollapsedTypecastNode(VuoRendererNode *uncollapsedTypecastNode);
	QString getTypecastTitle(void) const;
};

#endif // VUORENDERERTYPECASTPORT_HH