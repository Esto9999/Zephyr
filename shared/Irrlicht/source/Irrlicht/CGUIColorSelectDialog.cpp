// Copyright (C) 2002-2009 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIColorSelectDialog.h"

#ifdef _IRR_COMPILE_WITH_GUI_

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIButton.h"
#include "IGUIStaticText.h"
#include "IGUIFont.h"
#include "IGUISpriteBank.h"
#include "IFileList.h"
#include "os.h"
#include "CImage.h"

namespace irr
{
namespace gui
{

const s32 CSD_WIDTH = 350;
const s32 CSD_HEIGHT = 300;

struct sTemplate
{
	const wchar_t *pre;
	const wchar_t *init;
	const wchar_t *pos;
	int x, y;
	int range_down ,range_up;
};

static const sTemplate Template [] =
{
	{ L"A:", L"0", 0,20,165, 0, 255 },
	{ L"R:", L"0", 0,20,205, 0, 255 },
	{ L"G:", L"0", 0,20,230, 0, 255 },
	{ L"B:", L"0", 0,20,255, 0, 255 },
	{ L"H:", L"0", L"°",180,205, 0, 360 },
	{ L"S:", L"0", L"%",180,230, 0, 100 },
	{ L"L:", L"0", L"%",180,255, 0, 100 },
};


//! constructor
CGUIColorSelectDialog::CGUIColorSelectDialog(const wchar_t* title, IGUIEnvironment* environment, IGUIElement* parent, s32 id)
	: IGUIColorSelectDialog(environment, parent, id,
		core::rect<s32>((parent->getAbsolutePosition().getWidth()-CSD_WIDTH)/2,
			(parent->getAbsolutePosition().getHeight()-CSD_HEIGHT)/2,
			(parent->getAbsolutePosition().getWidth()-CSD_WIDTH)/2+CSD_WIDTH,
			(parent->getAbsolutePosition().getHeight()-CSD_HEIGHT)/2+CSD_HEIGHT)),
	Dragging(false)
{
	#ifdef _DEBUG
	IGUIElement::setDebugName("CGUIColorSelectDialog");
	#endif

	Text = title;

	IGUISkin* skin = Environment->getSkin();

	const s32 buttonw = environment->getSkin()->getSize(EGDS_WINDOW_BUTTON_WIDTH);
	const s32 posx = RelativeRect.getWidth() - buttonw - 4;

	CloseButton = Environment->addButton(core::rect<s32>(posx, 3, posx + buttonw, 3 + buttonw),
		this, -1, L"", skin ? skin->getDefaultText(EGDT_WINDOW_CLOSE) : L"Close");
	if (skin && skin->getSpriteBank())
	{
		CloseButton->setSpriteBank(skin->getSpriteBank());
		CloseButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_WINDOW_CLOSE), skin->getColor(EGDC_WINDOW_SYMBOL));
		CloseButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_WINDOW_CLOSE), skin->getColor(EGDC_WINDOW_SYMBOL));
	}
	CloseButton->setSubElement(true);
	CloseButton->setTabStop(false);
	CloseButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
	CloseButton->grab();

	OKButton = Environment->addButton(
		core::rect<s32>(RelativeRect.getWidth()-80, 30, RelativeRect.getWidth()-10, 50),
		this, -1, skin ? skin->getDefaultText(EGDT_MSG_BOX_OK) : L"OK");
	OKButton->setSubElement(true);
	OKButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
	OKButton->grab();

	CancelButton = Environment->addButton(
		core::rect<s32>(RelativeRect.getWidth()-80, 55, RelativeRect.getWidth()-10, 75),
		this, -1, skin ? skin->getDefaultText(EGDT_MSG_BOX_CANCEL) : L"Cancel");
	CancelButton->setSubElement(true);
	CancelButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
	CancelButton->grab();

	video::IVideoDriver* driver = Environment->getVideoDriver();
	ColorRing.Texture = driver->getTexture ( "#colorring" );
	if ( 0 == ColorRing.Texture )
	{
		buildColorRing(core::dimension2d<u32>(128, 128), 1,
			Environment->getSkin()->getColor(EGDC_3D_SHADOW));
	}

	core::rect<s32> r(20,20, 0,0);

	ColorRing.Control = Environment->addImage(ColorRing.Texture, r.UpperLeftCorner, true, this);
	ColorRing.Control->setSubElement(true);
	ColorRing.Control->grab();

	for ( u32 i = 0; i != sizeof (Template) / sizeof ( sTemplate ); ++i )
	{
		if ( Template[i].pre )
		{
			r.UpperLeftCorner.X = Template[i].x;
			r.UpperLeftCorner.Y = Template[i].y;
			r.LowerRightCorner.X = r.UpperLeftCorner.X + 15;
			r.LowerRightCorner.Y = r.UpperLeftCorner.Y + 20;
			IGUIElement *t = Environment->addStaticText(Template[i].pre, r, false, false, this);
			t->setSubElement(true);
		}

		if ( Template[i].pos )
		{
			r.UpperLeftCorner.X = Template[i].x + 52;
			r.UpperLeftCorner.Y = Template[i].y;
			r.LowerRightCorner.X = r.UpperLeftCorner.X + 15;
			r.LowerRightCorner.Y = r.UpperLeftCorner.Y + 20;
			IGUIElement *t = Environment->addStaticText( Template[i].pos, r, false, false, this);
			t->setSubElement(true);
		}

		SBatteryItem item;
		item.Incoming=0.f;
		item.Outgoing=0.f;

		r.UpperLeftCorner.X = Template[i].x + 15;
		r.UpperLeftCorner.Y = Template[i].y;
		r.LowerRightCorner.X = r.UpperLeftCorner.X + 35;
		r.LowerRightCorner.Y = r.UpperLeftCorner.Y + 20;

		item.Edit = Environment->addEditBox( Template[i].init, r, true, this);
		item.Edit->setSubElement(true);
		item.Edit->grab();

		r.UpperLeftCorner.X = Template[i].x + 70;
		r.UpperLeftCorner.Y = Template[i].y + 4;
		r.LowerRightCorner.X = r.UpperLeftCorner.X + 60;
		r.LowerRightCorner.Y = r.UpperLeftCorner.Y + 12;

		item.Scrollbar = Environment->addScrollBar(true, r, this);
		item.Scrollbar->grab ();
		item.Scrollbar->setSubElement(true);
		item.Scrollbar->setMax ( Template[i].range_up - Template[i].range_down );
		item.Scrollbar->setSmallStep ( 1 );

		Battery.push_back ( item );
	}

	bringToFront(CancelButton);
	bringToFront(OKButton);
}


//! destructor
CGUIColorSelectDialog::~CGUIColorSelectDialog()
{
	if (CloseButton)
		CloseButton->drop();

	if (OKButton)
		OKButton->drop();

	if (CancelButton)
		CancelButton->drop();

	for ( u32 i = 0; i != Battery.size ();++i )
	{
		Battery[i].Edit->drop();
		Battery[i].Scrollbar->drop();
	}

	if (ColorRing.Control)
		ColorRing.Control->drop();
}


//! renders a antialiased, colored ring
void CGUIColorSelectDialog::buildColorRing( const core::dimension2d<u32> & dim, s32 supersample, const video::SColor& borderColor )
{
	const core::dimension2d<u32> d(dim.Width * supersample, dim.Height * supersample);
	video::CImage *RawTexture = new video::CImage(video::ECF_A8R8G8B8, d);

	RawTexture->fill ( 0x00808080 );

	const s32 radiusOut = ( d.Width / 2 ) - 4;
	const s32 fullR2 = radiusOut * radiusOut;

	video::SColor rgb(0xFF000000);
	video::SColorHSL hsl;
	hsl.Luminance = 0.5f;
	hsl.Saturation = 1.f;

	core::position2d<s32> p;
	for ( p.Y = -radiusOut;  p.Y <= radiusOut;  p.Y += 1  )
	{
		s32 y2 = p.Y * p.Y;

		for (p.X = -radiusOut; p.X <= radiusOut; p.X += 1)
		{
			s32 r2 = y2 + ( p.X * p.X );

			// test point in circle
			s32 testa = r2 - fullR2;

			if ( testa < 0 )
			{
				// dotproduct u ( x,y ) * v ( 1, 0 ) = cosinus(a)

				const f32 r = sqrtf((f32) r2);

				// normalize, dotproduct = xnorm
				const f32 xn = r == 0.f ? 0.f : -p.X * core::reciprocal(r);

				hsl.Hue = acosf(xn);
				if ( p.Y > 0 )
					hsl.Hue = (2.f * core::PI ) - hsl.Hue;

				hsl.Hue -= core::PI / 2.f;

				const f32 rTest = r / radiusOut;
/*
				if ( rTest < 0.25f )
				{
					hsl.Luminance = rTest / 0.25f;
					hsl.Saturation = 0.f;
					hsl.toRGB  ( rgb );
					*dst = rgb.color;
				}
				else
				if ( rTest < 0.4f )
				{
					hsl.Saturation = ( rTest - 0.25f ) / 0.15f;
					hsl.Luminance = 1.f - ( hsl.Saturation / 2.4f );
					hsl.Luminance = 0.5f;
					hsl.toRGB  ( rgb );
					// *dst = rgb.color;
				}
				else
				if ( rTest < 0.75f )
				{
					hsl.Luminance = 0.5f;
					hsl.Saturation = 1.f;
					hsl.toRGB  ( rgb );
					*dst = rgb.color;
				}
				else
				if ( rTest < 0.98f )
				{
					hsl.Luminance = 0.5f - ( ( rTest - 0.75f ) / 0.75f );
					hsl.Saturation = 1.f;
					hsl.toRGB  ( rgb );
					*dst = rgb.color;
				}
*/

				if ( rTest >= 0.5f )
				{
					hsl.Luminance = 0.5f;
					hsl.Saturation = 1.f;
					hsl.toRGB(rgb);

					if ( rTest <= 0.55f )
					{
						const u32 alpha = (u32) ( (rTest - 0.5f ) * ( 255.f / 0.05f ) );
						rgb.setAlpha(alpha);
					} 
					else if ( rTest >= 0.95f )
					{
						const u32 alpha = (u32) ( (rTest - 0.95f ) * ( 255.f / 0.05f ) );
						rgb.setAlpha(255-alpha);
					}
					else
						rgb.setAlpha(255);
					RawTexture->setPixel(4+p.X+radiusOut, 4+p.Y+radiusOut, rgb);
				}
			}
		}
	}

	RawTexture->unlock ();

	if ( supersample > 1 )
	{
		video::CImage * filter = new video::CImage(video::ECF_A8R8G8B8, dim );
		RawTexture->copyToScalingBoxFilter(filter);
		RawTexture->drop();
		RawTexture = filter;
	}

	video::IVideoDriver* driver = Environment->getVideoDriver();

	bool generateMipLevels = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag( video::ETCF_CREATE_MIP_MAPS, false);

	ColorRing.Texture = driver->addTexture ( "#colorring", RawTexture);
	RawTexture->drop();

	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, generateMipLevels);
}


//! called if an event happened.
bool CGUIColorSelectDialog::OnEvent(const SEvent& event)
{
	if (IsEnabled)
	{
		switch(event.EventType)
		{
			case EET_GUI_EVENT:
			switch(event.GUIEvent.EventType)
			{
				case EGET_SCROLL_BAR_CHANGED:
				{
					for ( u32 i = 0; i!= Battery.size (); ++i )
					{
						if ( event.GUIEvent.Caller == Battery[i].Scrollbar )
						{
							const s32 value = Template[i].range_down + Battery[i].Scrollbar->getPos();
							Battery[i].Edit->setText(core::stringw(value).c_str());
						}
					}
					return true;
				}

			case EGET_ELEMENT_FOCUS_LOST:
				Dragging = false;
				break;
			case EGET_BUTTON_CLICKED:
				if (event.GUIEvent.Caller == CloseButton ||
					event.GUIEvent.Caller == CancelButton)
				{
					sendCancelEvent();
					remove();
					return true;
				}
				else
				if (event.GUIEvent.Caller == OKButton)
				{
					sendSelectedEvent();
					remove();
					return true;
				}
				break;

			case EGET_LISTBOX_CHANGED:
			case EGET_LISTBOX_SELECTED_AGAIN:
			default:
				break;
			}
			break;
		case EET_MOUSE_INPUT_EVENT:
			switch(event.MouseInput.Event)
			{
			case EMIE_LMOUSE_PRESSED_DOWN:
				DragStart.X = event.MouseInput.X;
				DragStart.Y = event.MouseInput.Y;
				Dragging = true;
				Environment->setFocus(this);
				return true;
			case EMIE_LMOUSE_LEFT_UP:
				Dragging = false;
				Environment->removeFocus(this);
				return true;
			case EMIE_MOUSE_MOVED:
				if (Dragging)
				{
					// gui window should not be dragged outside its parent
					if (Parent)
						if (event.MouseInput.X < Parent->getAbsolutePosition().UpperLeftCorner.X +1 ||
							event.MouseInput.Y < Parent->getAbsolutePosition().UpperLeftCorner.Y +1 ||
							event.MouseInput.X > Parent->getAbsolutePosition().LowerRightCorner.X -1 ||
							event.MouseInput.Y > Parent->getAbsolutePosition().LowerRightCorner.Y -1)

							return true;

					move(core::position2d<s32>(event.MouseInput.X - DragStart.X, event.MouseInput.Y - DragStart.Y));
					DragStart.X = event.MouseInput.X;
					DragStart.Y = event.MouseInput.Y;
					return true;
				}
			default:
				break;
			}
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}


//! draws the element and its children
void CGUIColorSelectDialog::draw()
{
	if (!IsVisible)
		return;

	IGUISkin* skin = Environment->getSkin();
	core::rect<s32> rect = skin->draw3DWindowBackground(this, true, skin->getColor(EGDC_ACTIVE_BORDER),
		AbsoluteRect, &AbsoluteClippingRect);

	if (Text.size())
	{
		rect.UpperLeftCorner.X += 2;
		rect.LowerRightCorner.X -= skin->getSize(EGDS_WINDOW_BUTTON_WIDTH) + 5;

		IGUIFont* font = skin->getFont(EGDF_WINDOW);
		if (font)
			font->draw(Text.c_str(), rect, skin->getColor(EGDC_ACTIVE_CAPTION), false, true,
			&AbsoluteClippingRect);
	}
	IGUIFont* font = Environment->getBuiltInFont();
	if (font)
		font->draw(L"+", core::rect<s32>(20,20,50,50), video::SColor(), false, false,
		&AbsoluteClippingRect);

	IGUIElement::draw();
}


//! sends the event that the file has been selected.
void CGUIColorSelectDialog::sendSelectedEvent()
{
	SEvent event;
	event.EventType = EET_GUI_EVENT;
	event.GUIEvent.Caller = this;
	event.GUIEvent.Element = 0;
	event.GUIEvent.EventType = EGET_FILE_SELECTED;
	Parent->OnEvent(event);
}


//! sends the event that the file choose process has been canceld
void CGUIColorSelectDialog::sendCancelEvent()
{
	SEvent event;
	event.EventType = EET_GUI_EVENT;
	event.GUIEvent.Caller = this;
	event.GUIEvent.Element = 0;
	event.GUIEvent.EventType = EGET_FILE_CHOOSE_DIALOG_CANCELLED;
	Parent->OnEvent(event);
}


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

