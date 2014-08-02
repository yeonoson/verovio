/////////////////////////////////////////////////////////////////////////////
// Name:        iomei.cpp
// Author:      Laurent Pugin
// Created:     2008
// Copyright (c) Laurent Pugin. All rights reserved.
/////////////////////////////////////////////////////////////////////////////


#include "iomei.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <assert.h>
#include <ctime>
#include <sstream>

//----------------------------------------------------------------------------

#include "vrv.h"
#include "app.h"
#include "barline.h"
#include "beam.h"
#include "clef.h"
#include "keysig.h"
#include "layer.h"
#include "layerelement.h"
#include "measure.h"
#include "mensur.h"
#include "metersig.h"
#include "mrest.h"
#include "multirest.h"
#include "note.h"
#include "page.h"
#include "rest.h"
#include "scoredef.h"
#include "staff.h"
#include "symbol.h"
#include "system.h"
#include "tuplet.h"

namespace vrv {

//----------------------------------------------------------------------------
// MeiOutput
//----------------------------------------------------------------------------

MeiOutput::MeiOutput( Doc *doc, std::string filename ) :
	FileOutputStream( doc )
{
    m_filename = filename;
}

MeiOutput::~MeiOutput()
{
}

bool MeiOutput::ExportFile( )
{
    try {
        pugi::xml_document meiDoc;
        
        m_mei = meiDoc.append_child("mei");
        m_mei.append_attribute( "xmlns" ) = "http://www.music-encoding.org/ns/mei";
        m_mei.append_attribute( "meiversion" ) = "2013";

        pugi::xml_node music = m_mei.append_child("music");
        pugi::xml_node body = music.append_child("body");
        pugi::xml_node mdiv = body.append_child("mdiv");
        
        
        // element to place the pages
        m_pages = mdiv.append_child("pages");
        m_pages.append_attribute( "type" ) = DocTypeToStr( m_doc->GetType() ).c_str();
        
        // this starts the call of all the functors
        m_doc->Save( this );
        
        /*
        TiXmlUnknown *schema = new TiXmlUnknown();
        schema->SetValue("?xml-model href=\"http://www.aruspix.net/mei-page-based-2013-08-29.rng\" type=\"application/xml\" schematypens=\"http://relaxng.org/ns/structure/1.0\"?");
        
        meiDoc->LinkEndChild( new TiXmlDeclaration( "1.0", "UTF-8", "" ) );
        meiDoc->LinkEndChild(schema);
        */
        meiDoc.save_file( m_filename.c_str() );
    }
    catch( char * str ) {
        LogError("%s", str );
        return false;
    }
	return true;    
}

std::string MeiOutput::UuidToMeiStr( Object *element )
{
    std::string out = element->GetUuid();
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    LogDebug("uuid: %s\n", out.c_str());
    return out;
}

bool MeiOutput::WriteDoc( Doc *doc )
{
    assert( !m_mei.empty() );
    
    // ---- header ----
    pugi::xml_node meiHead = m_mei.append_child("meiHead");
    
    pugi::xml_node fileDesc = meiHead.append_child("fileDesc");
    pugi::xml_node titleStmt = fileDesc.append_child("titleStmt");
    titleStmt.append_child("title");
    pugi::xml_node pubStmt = fileDesc.append_child("pubStmt");
    pugi::xml_node date = pubStmt.append_child("date");
    
    pugi::xml_node encodingDesc = meiHead.append_child("encodingDesc");
    pugi::xml_node projectDesc = encodingDesc.append_child("projectDesc");
    pugi::xml_node p1 = projectDesc.append_child("p");
    p1.append_child(pugi::node_pcdata).set_value( StringFormat( "Encoded with Verovio version %s",  GetVersion().c_str() ).c_str() );
    
    // date
    time_t now = time(0);
    date.append_child(pugi::node_pcdata).set_value( ctime( &now ) );
    
    return true;
}


bool MeiOutput::WritePage( Page *page )
{
    assert( !m_pages.empty() );
    
    m_page = m_pages.append_child("page");
    m_page.append_attribute( "xml:id" ) =  UuidToMeiStr( page ).c_str();
    // size and margins but only if any - we rely on page.height only to check this
    if ( page->m_pageHeight != -1 ) {
        m_page.append_attribute( "page.width" ) = StringFormat( "%d", page->m_pageWidth ).c_str();
        m_page.append_attribute( "page.height" ) = StringFormat( "%d", page->m_pageHeight ).c_str();
        m_page.append_attribute( "page.leftmar" ) = StringFormat( "%d", page->m_pageLeftMar ).c_str();
        m_page.append_attribute( "page.rightmar" ) = StringFormat( "%d", page->m_pageRightMar ).c_str();
        m_page.append_attribute( "page.rightmar" ) = StringFormat( "%d", page->m_pageRightMar ).c_str();
    }
    if ( !page->m_surface.empty() ) {
        m_page.append_attribute( "surface" ) = page->m_surface.c_str();
    }
    //
    m_pages.append_child(pugi::node_comment).set_value( "Coordinates in MEI axis direction" );
    
    return true;
}

bool MeiOutput::WriteSystem( System *system )
{
    assert( !m_page.empty() );
    m_system = m_page.append_child("system");
    m_system.append_attribute( "xml:id" ) =  UuidToMeiStr( system ).c_str();
    // margins
    m_system.append_attribute( "system.leftmar" ) = StringFormat( "%d", system->m_systemLeftMar ).c_str();
    m_system.append_attribute( "system.rightmar") = StringFormat( "%d", system->m_systemRightMar ).c_str();
    // y positions
    m_system.append_attribute( "uly" ) = StringFormat( "%d", system->m_yAbs ).c_str();
    
    return true;
}

bool MeiOutput::WriteScoreDef( ScoreDef *scoreDef )
{
    assert( !m_system.empty() );
    m_scoreDef = m_system.append_child("scoreDef");
    m_scoreDef.append_attribute( "xml:id" ) =  UuidToMeiStr( scoreDef ).c_str();
    if (scoreDef->GetClefAttr()) {
        m_scoreDef.append_attribute( "clef.line" ) = ClefLineToStr( scoreDef->GetClefAttr()->m_clefId ).c_str();
        m_scoreDef.append_attribute( "clef.shape" ) = ClefShapeToStr( scoreDef->GetClefAttr()->m_clefId ).c_str();
        // we should add 8va attr
    }
    if (scoreDef->GetKeySigAttr()) {
        m_scoreDef.append_attribute( "key.sig" ) = KeySigToStr( scoreDef->GetKeySigAttr()->GetAlterationNumber(),
                                                         scoreDef->GetKeySigAttr()->GetAlteration() ).c_str();
    }
    
    // this needs to be fixed
    return true;
    
}

bool MeiOutput::WriteStaffGrp( StaffGrp *staffGrp )
{
    // for now only as part of a system - this needs to be fixed
    assert( !m_system.empty() );
    
    m_staffGrp = m_system.append_child("staffGrp");
    m_staffGrp.append_attribute( "xml:id" ) = UuidToMeiStr( staffGrp ).c_str();
    if ( staffGrp->GetSymbol() != STAFFGRP_NONE ) {
        m_staffGrp.append_attribute( "symbol" ) = StaffGrpSymbolToStr( staffGrp->GetSymbol() ).c_str();
    }
    if ( staffGrp->GetBarthru() ) {
        m_staffGrp.append_attribute( "barthru" ) = BoolToStr( staffGrp->GetBarthru() ).c_str();
    }
    
    return true;
}

bool MeiOutput::WriteStaffDef( StaffDef *staffDef )
{
    assert( !m_staffGrp.empty() );
    
    m_staffDef = m_staffGrp.append_child("staffDef");
    m_staffDef.append_attribute( "xml:id" ) =  UuidToMeiStr( staffDef ).c_str();
    m_staffDef.append_attribute( "n" ) = StringFormat( "%d", staffDef->GetStaffNo() ).c_str();
    if (staffDef->GetClefAttr()) {
        m_staffDef.append_attribute( "clef.line" ) = ClefLineToStr( staffDef->GetClefAttr()->m_clefId ).c_str();
        m_staffDef.append_attribute( "clef.shape" ) = ClefShapeToStr( staffDef->GetClefAttr()->m_clefId ).c_str();
        // we should add 8va attr
    }
    if (staffDef->GetKeySigAttr()) {
        m_staffDef.append_attribute( "key.sig" ) = KeySigToStr( staffDef->GetKeySigAttr()->GetAlterationNumber(),
                                                         staffDef->GetKeySigAttr()->GetAlteration() ).c_str();
    }
    
    return true;
}
      
bool MeiOutput::WriteMeasure( Measure *measure )
{
    assert( !m_system.empty() );
    
    m_measure = m_system.append_child("measure");
    m_measure.append_attribute( "xml:id" ) =  UuidToMeiStr( measure ).c_str();
    //m_measure.append_attribute( "n" ) = StringFormat( "%d", measure->m_logMeasureNb ).c_str();
    WriteAttCommon( m_measure, measure );
    
    return true;
}

bool MeiOutput::WriteStaff( Staff *staff )
{
    assert( !m_measure.empty() );
    
    m_staff = m_measure.append_child("staff");
    m_staff.append_attribute( "xml:id" ) =  UuidToMeiStr( staff ).c_str();
    // y position
    if ( staff->notAnc ) {
        m_staff.append_attribute( "label" ) = "mensural";
    }
    m_staff.append_attribute( "uly" ) = StringFormat( "%d", staff->m_yAbs ).c_str();
    m_staff.append_attribute( "n" ) = StringFormat( "%d", staff->GetStaffNo() ).c_str();

    return true;
}


bool MeiOutput::WriteLayer( Layer *layer )
{
    assert( !m_staff.empty() );
    m_layer = m_staff.append_child("layer");
    m_layer.append_attribute( "xml:id" ) =  UuidToMeiStr( layer ).c_str();
    m_layer.append_attribute( "n" ) = StringFormat( "%d", layer->GetLayerNo() ).c_str();
    return true;
}

bool MeiOutput::WriteLayerElement( LayerElement *element )
{
    assert( !m_layer.empty() );
    
    // Here we look at what is the parent.
    // For example, if we are in a beam, we vrvT attach it to the beam xml element (m_beam)
    // By default, we attach it to m_layer
    pugi::xml_node currentParent = m_layer;
    if ( dynamic_cast<LayerRdg*>(element->m_parent) ) {
        assert( m_rdgLayer );
        currentParent = m_rdgLayer;
    }
    else if ( dynamic_cast<Beam*>(element->m_parent) ) {
        assert( m_beam );
        currentParent = m_beam;
    }
    else if ( dynamic_cast<Tuplet*>(element->m_parent) ) {
        assert( m_tuplet );
        currentParent = m_tuplet;
    }
    // we should do the same for any LayerElement container (slur, tuplet, etc. )
    
    pugi::xml_node xmlElement;
    if (dynamic_cast<Barline*>(element)) {
        xmlElement = currentParent.append_child( "barline" );
        WriteMeiBarline( xmlElement, dynamic_cast<Barline*>(element) );
    }
    else if (dynamic_cast<Beam*>(element)) {
        xmlElement = currentParent.append_child("beam");
        m_beam = xmlElement;
        WriteMeiBeam( xmlElement, dynamic_cast<Beam*>(element) );
    }
    else if (dynamic_cast<Clef*>(element)) {
        xmlElement = currentParent.append_child("clef");
        WriteMeiClef( xmlElement, dynamic_cast<Clef*>(element) );
    }
    else if (dynamic_cast<Mensur*>(element)) {
        xmlElement = currentParent.append_child("mensur");
        WriteMeiMensur( xmlElement, dynamic_cast<Mensur*>(element) );
    }
    else if (dynamic_cast<MRest*>(element)) {
        xmlElement = currentParent.append_child("mRest");
        WriteMeiMRest( xmlElement, dynamic_cast<MRest*>(element) );
    }
    else if (dynamic_cast<MultiRest*>(element)) {
        xmlElement = currentParent.append_child("multiRest");
        WriteMeiMultiRest( xmlElement, dynamic_cast<MultiRest*>(element) );
    }
    else if (dynamic_cast<Note*>(element)) {
        xmlElement = currentParent.append_child("note");
        WriteMeiNote( xmlElement, dynamic_cast<Note*>(element) );
    }
    else if (dynamic_cast<Rest*>(element)) {
        xmlElement = currentParent.append_child("rest");
        WriteMeiRest( xmlElement, dynamic_cast<Rest*>(element) );
    }
    else if (dynamic_cast<Tuplet*>(element)) {
        xmlElement = currentParent.append_child("tuplet");
        m_tuplet = xmlElement;
        WriteMeiTuplet( xmlElement, dynamic_cast<Tuplet*>(element) );
    }
    else if (dynamic_cast<Symbol*>(element)) {        
        xmlElement = WriteMeiSymbol( dynamic_cast<Symbol*>(element), currentParent );
    }
    
    // we have it, set the uuid we read
    if ( !xmlElement.empty() ) {
        this->WriteSameAsAttr( xmlElement, element );
        xmlElement.append_attribute( "xml:id" ) =  UuidToMeiStr( element ).c_str();
        if ( element->m_xAbs != VRV_UNSET) {
            xmlElement.append_attribute( "ulx" ) = StringFormat( "%d", element->m_xAbs ).c_str();
        }
        return true;
    }
    else {
        LogWarning( "Element class %s could not be saved", element->GetClassName().c_str() );
        return false;
    }    
}

void MeiOutput::WriteMeiBarline( pugi::xml_node meiBarline, Barline *barline )
{
    return;
}


void MeiOutput::WriteMeiBeam( pugi::xml_node meiBeam, Beam *beam )
{
    return;
}


void MeiOutput::WriteMeiClef( pugi::xml_node meiClef, Clef *clef )
{
    meiClef.append_attribute( "line" ) = ClefLineToStr( clef->m_clefId ).c_str();
    meiClef.append_attribute( "shape" ) = ClefShapeToStr( clef->m_clefId ).c_str();
    // we should add 8va attr
    return;
}


void MeiOutput::WriteMeiMensur( pugi::xml_node meiMensur, Mensur *mensur )
{
    if ( mensur->m_sign ) {
        meiMensur.append_attribute( "sign" ) = MensurationSignToStr( mensur->m_sign ).c_str();
    }
    if ( mensur->m_dot ) {
        meiMensur.append_attribute( "dot" ) = "true";
    }
    if ( mensur->m_slash ) {
        meiMensur.append_attribute( "slash" ) = "1"; // only one slash for now
    }
    if ( mensur->m_reversed ) {
        meiMensur.append_attribute( "orient" ) = "reversed"; // only orientation
    }
    if ( mensur->m_num ) {
        meiMensur.append_attribute( "num" ) = StringFormat("%d", mensur->m_num ).c_str();
    }
    if ( mensur->m_numBase ) {
        meiMensur.append_attribute( "numbase" ) = StringFormat("%d", mensur->m_numBase ).c_str();
    }
    // missing m_meterSymb
    
    return;
}

void MeiOutput::WriteMeiMeterSig( pugi::xml_node meiMeterSig, MeterSig *meterSig )
{
    WriteAttMeterSigLog( meiMeterSig, meterSig );
    return;
}
    
    
void MeiOutput::WriteMeiMRest( pugi::xml_node meiMRest, MRest *mRest )
{
     return;
}

void MeiOutput::WriteMeiMultiRest( pugi::xml_node meiMultiRest, MultiRest *multiRest )
{
    meiMultiRest.append_attribute( "num" ) = StringFormat("%d", multiRest->GetNumber()).c_str();

    return;
}

void MeiOutput::WriteMeiNote( pugi::xml_node meiNote, Note *note )
{
    meiNote.append_attribute( "pname" ) = PitchToStr( note->m_pname ).c_str();
    meiNote.append_attribute( "oct" ) = OctToStr( note->m_oct ).c_str();
    meiNote.append_attribute( "dur" ) = DurToStr( note->m_dur ).c_str();
    if (note->m_durGes != VRV_UNSET) {
        meiNote.append_attribute( "dur.ges" ) = DurToStr( note->m_durGes ).c_str();
    }
    if ( note->m_dots ) {
        meiNote.append_attribute( "dots" ) = StringFormat("%d", note->m_dots).c_str();
    }
    if ( note->m_accid ) {
        meiNote.append_attribute( "accid" ) = AccidToStr( note->m_accid ).c_str();
    }
    if ( note->m_cueSize ) {
        meiNote.append_attribute( "grace" ) = "unknown";
    }
    if ( note->m_lig ) {
        if ( note->m_ligObliqua ) {
            meiNote.append_attribute( "lig" ) = "obliqua";
        }
        else {
            meiNote.append_attribute( "lig" ) = "recta";
        }
    }
    if ( note->m_stemDir ) {
        // this is not really correct because Note::m_stemDir indicates that it is opposite the normal position
        meiNote.append_attribute( "stem.dir" ) = "up";
    }
    if ( note->m_colored ) {
        meiNote.append_attribute( "colored" ) = "true";
    }
    // missing m_artic, m_chord, m_headShape, m_slur, m_stemLen
    return;
}

void MeiOutput::WriteMeiRest( pugi::xml_node meiRest, Rest *rest )
{    
    meiRest.append_attribute( "dur" ) = DurToStr( rest->m_dur ).c_str();
    if ( rest->m_dots ) {
        meiRest.append_attribute( "dots" ) = StringFormat("%d", rest->m_dots).c_str();
    }
    // missing position
    meiRest.append_attribute( "ploc" ) = PitchToStr( rest->m_pname ).c_str();
    meiRest.append_attribute( "oloc" ) = OctToStr( rest->m_oct ).c_str();
    return;
}

pugi::xml_node MeiOutput::WriteMeiSymbol( Symbol *symbol, pugi::xml_node currentParent )
{
    pugi::xml_node xmlElement = currentParent.append_child();
    if (symbol->m_type==SYMBOL_ACCID) {
        xmlElement.set_value("accid");
        xmlElement.append_attribute( "accid" ) = AccidToStr( symbol->m_accid ).c_str();
        // position
        xmlElement.append_attribute( "ploc" ) = PitchToStr( symbol->m_pname ).c_str();
        xmlElement.append_attribute( "oloc" ) = OctToStr( symbol->m_oct ).c_str();
    }
    else if (symbol->m_type==SYMBOL_CUSTOS) {
        xmlElement.set_value("custos");
        xmlElement.append_attribute( "pname" ) = PitchToStr( symbol->m_pname ).c_str();
        xmlElement.append_attribute( "oct" ) = OctToStr( symbol->m_oct ).c_str();
    }
    else if (symbol->m_type==SYMBOL_DOT) {
        xmlElement.set_value("dot");
        // missing m_dots
        // position
        xmlElement.append_attribute( "ploc" ) = PitchToStr( symbol->m_pname ).c_str();
        xmlElement.append_attribute( "oloc" ) = OctToStr( symbol->m_oct ).c_str();
    }
    return xmlElement;
}


void MeiOutput::WriteMeiTuplet( pugi::xml_node meiTuplet, Tuplet *tuplet )
{
    return;
}

bool MeiOutput::WriteLayerApp( LayerApp *app )
{    
    assert( !m_layer.empty() );
    
    m_app = m_layer.append_child("app");
    
    return true;
}

bool MeiOutput::WriteLayerRdg( LayerRdg *rdg )
{   
    assert( !m_app.empty() );
    
    m_rdgLayer = m_app.append_child("rdg");
    m_rdgLayer.append_attribute( "source" ) = rdg->m_source.c_str();

    return true;
}


void MeiOutput::WriteSameAsAttr( pugi::xml_node element, Object *object )
{
    if ( !object->m_sameAs.empty() ) {
        element.append_attribute( "sameas" ) = object->m_sameAs.c_str();
    }
}

void MeiOutput::WriteAttCommon( pugi::xml_node element, Object *object )
{
    AttCommon *common = dynamic_cast<AttCommon*>( object );
    assert( common );
    if ( !common->GetLabel().empty() ) {
        element.append_attribute( "label" ) = common->GetLabel().c_str();
    }
    if ( common->GetN() != VRV_UNSET ) {
        element.append_attribute( "n" ) = StringFormat("%d", common->GetN()).c_str();;
    }
}
    
void MeiOutput::WriteAttMeterSigLog( pugi::xml_node element, Object *object, bool isMeterSigLogDefault )
{
    std::string prefix = "";
    if ( isMeterSigLogDefault ) {
        prefix = "meter.";
    }
    AttMeterSigLog *att = dynamic_cast<AttMeterSigLog*>( object );
    assert( att );
    if ( att->GetCount() != 0 ) {
        element.append_attribute( (prefix + "count").c_str() ) = StringFormat("%d", att->GetCount()).c_str();;
    }
    if ( att->GetSym() != METERSIGN_NONE ) {
        element.append_attribute( (prefix + "sym").c_str() ) = MeterSignToStr( att->GetSym() ).c_str();
    }
    if ( att->GetUnit() != 0 ) {
        element.append_attribute( (prefix + "unit").c_str() ) = StringFormat("%d", att->GetUnit()).c_str();;
    }
    return;
}

    
std::string MeiOutput::BoolToStr(bool value)
{
    if (value) return "true";
    return "false";
}

std::string MeiOutput::OctToStr(int oct)
{
	char buf[3];
	snprintf(buf, 2, "%d", oct);
	return std::string(buf);
	
	// For some reason, #include <sstream> does not work with xcode 3.2
	//std::ostringstream oss;
	//oss << oct;
	//return oss.str();    
}


std::string MeiOutput::PitchToStr(int pitch)
{
    std::string value;
    switch (pitch) {
        case 7:
        case 0: value = "b"; break;
        case 1: value = "c"; break;
        case 2: value = "d"; break;
        case 3: value = "e"; break;
        case 4: value = "f"; break;
        case 5: value = "g"; break;
        case 6: value = "a"; break;
        default: 
            LogWarning("Unknown pitch '%d'", pitch);
            value = "";
            break;
    }
	return value;
}

std::string MeiOutput::AccidToStr(unsigned char accid)
{
    std::string value;
    switch (accid) {
        case ACCID_SHARP: value = "s"; break;
        case ACCID_FLAT: value = "f"; break;
        case ACCID_NATURAL: value = "n"; break;
        case ACCID_DOUBLE_SHARP: value = "x"; break;
        case ACCID_DOUBLE_FLAT: value = "ff"; break;
        case ACCID_QUARTER_SHARP: value = "ns"; break;
        case ACCID_QUARTER_FLAT: value = "nf"; break;
        default: 
            LogWarning("Unknown accid '%d'", accid);
            value = "";
            break;
    }
	return value;
}

std::string MeiOutput::ClefLineToStr( ClefId clefId )
{	
	std::string value; 
	switch(clefId)
	{	
        case SOL2 : value = "2"; break;
		case SOL1 : value = "1"; break; 
		case SOLva : value = "2"; break;
		case FA5 : value = "5"; break;
		case FA4 : value = "4"; break;
		case FA3 : value = "3"; break;
		case UT1 : value = "1"; break;
        case UT2 : value = "2"; break;
		case UT3 : value = "3"; break;
		case UT4 : value = "4"; break;
		case UT5 : value = "5"; break;
        default: 
            LogWarning("Unknown clef '%d'", clefId);
            value = "";
            break;
	}
	return value;
}

std::string MeiOutput::ClefShapeToStr( ClefId clefId )
{	
	std::string value; 
	switch(clefId)
	{	
        case SOL2 : 
		case SOL1 : 
		case SOLva : value = "G"; break;
		case FA5 : 
		case FA4 :
		case FA3 : value = "F"; break;
        case UT1 :
		case UT2 : 
		case UT3 : 
		case UT4 : 
		case UT5 : value = "C"; break;		
        default: 
            LogWarning("Unknown clef '%d'", clefId);
            value = "";
            break;
	}
	return value;
}

std::string MeiOutput::MensurationSignToStr(MensurationSign sign)
{
 	std::string value; 
	switch(sign)
	{	case MENSURATIONSIGN_C : value = "C"; break;
		case MENSURATIONSIGN_O : value = "O"; break;		
        default: 
            LogWarning("Unknown mensur sign '%d'", sign);
            value = "";
            break;
	}
	return value;   
}

    
std::string MeiOutput::MeterSignToStr(MeterSign sign)
{
    std::string value;
    switch(sign)
    {	case METERSIGN_COMMON : value = "common"; break;
        case METERSIGN_CUT : value = "cut"; break;
        default:
            LogWarning("Unknown meterSig sym '%d'", sign);
            value = "";
            break;
    }
    return value;   
}

std::string MeiOutput::DurToStr( int dur )
{
    std::string value;
    if (dur == DUR_LG) value = "longa";
    else if (dur == DUR_BR) value = "brevis";
    else if (dur == DUR_1) value = "semibrevis";
    else if (dur == DUR_2) value = "minima";
    else if (dur == DUR_4) value = "semiminima";
    else if (dur == DUR_8) value = "fusa";
    else if (dur == DUR_16) value = "semifusa";
    else if (dur == DUR_LG) value = "long";
    else if (dur == DUR_BR) value = "breve";
    else if (dur == DUR_1) value = "1";
    else if (dur == DUR_2) value = "2";
    else if (dur == DUR_4) value = "4";
    else if (dur == DUR_8) value = "8";
    else if (dur == DUR_16) value = "16";
    else if (dur == DUR_32) value = "32";
    else if (dur == DUR_64) value = "64";
    else if (dur == DUR_128) value = "128";
	else {
		LogWarning("Unknown duration '%d'", dur);
        value = "4";
	}
    return value;
}

std::string MeiOutput::DocTypeToStr(DocType type)
{
 	std::string value; 
	switch(type)
	{	
        case Raw : value = "raw"; break;
        case Rendering : value = "rendering"; break;
		case Transcription : value = "transcription"; break;		
        default: 
            LogWarning("Unknown document type '%d'", type);
            value = "";
            break;
	}
	return value;   
}


std::string MeiOutput::KeySigToStr(int num, char alter_type )
{
 	std::string value;
    if (num == 0) {
        return "0";
    }
	switch(alter_type)
	{	case ACCID_FLAT : value = StringFormat("%df", num); break;
		case ACCID_SHARP : value = StringFormat("%ds", num); break;
        default:
            LogWarning("Unknown key signature values '%d' and '%d", num, alter_type);
            value = "0";
            break;
	}
	return value;
}


std::string MeiOutput::StaffGrpSymbolToStr(StaffGrpSymbol symbol)
{
 	std::string value;
	switch(symbol)
	{	case STAFFGRP_LINE : value = "line"; break;
		case STAFFGRP_BRACE : value = "brace"; break;
        case STAFFGRP_BRACKET : value = "bracket"; break;
        default:
            LogWarning("Unknown staffGrp @symbol  '%d'", symbol);
            value = "line";
            break;
	}
	return value;
}


//----------------------------------------------------------------------------
// MeiInput
//----------------------------------------------------------------------------

MeiInput::MeiInput( Doc *doc, std::string filename ) :
	FileInputStream( doc )
{
    m_filename = filename;
    m_doc->m_fname = GetFilename( filename );
    m_page = NULL;
    m_scoreDef = NULL;
    m_staffDef = NULL;
    m_system = NULL;
	m_staff = NULL;
    m_measure = NULL;
	m_layer = NULL;
    m_layerApp = NULL;
    m_layerRdg = NULL;
    m_beam = NULL;
    m_tuplet = NULL;
    //
    m_currentLayer = NULL;
    //
    m_hasScoreDef = false;
}

MeiInput::~MeiInput()
{
}

bool MeiInput::ImportFile( )
{
    try {
        m_doc->Reset( Raw );
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file( m_filename.c_str() );
        if (!result)
        { 
            return false;
        }
        pugi::xml_node root = doc.first_child();
        return ReadMei( root );
        }
    catch( char * str ) {
        LogError("%s", str );
        return false;
    }
}

bool MeiInput::ImportString( const std::string mei )
{
    try {
        m_doc->Reset( Raw );
        pugi::xml_document doc;
        doc.load( mei.c_str() );
        pugi::xml_node root = doc.first_child();
        return ReadMei( root );
    }
    catch( char * str ) {
        LogError("%s", str );
        return false;
    }
}



bool MeiInput::ReadMei( pugi::xml_node root )
{
    pugi::xml_node current;
    
    if ( !root.empty() && (current = root.child("meiHead" ) ) )
    {
        ReadMeiHeader( current );
    }
    // music
    pugi::xml_node music;
    pugi::xml_node body;
    pugi::xml_node mdiv;
    pugi::xml_node pages;
    if ( !root.empty() ) {
        music = root.child("music");
    }
    if ( !music.empty() ) {
        body = music.child("body");
    }
    if ( !body.empty() ) {
        mdiv = body.child("mdiv");
    }
    if ( !mdiv.empty() ) {
        pages = mdiv.child("pages");
    }
    if ( !pages.empty() ) {
        
        // check if there is a type attribute for the score
        DocType type;
        if ( pages.attribute( "type" ) ) {
            type = StrToDocType( pages.attribute( "type" ).value() );
            m_doc->Reset( type );
        }
        
        // this is a page-based MEI file, we just loop trough the pages
        if ( pages.child( "page" ) ) {
            // because we are in a page-based MEI
            this->m_hasLayoutInformation = true;
            for( current = pages.child( "page" ); current; current = current.next_sibling( "page" ) ) {
                m_page = new Page( );
                SetMeiUuid( current, m_page );
                if (ReadMeiPage( current )) {
                    m_doc->AddPage( m_page );
                }
                else {
                    delete m_page;
                }
                m_page = NULL;
            }
        }
    }
    else {
        m_page = new Page( );
        m_system = new System( );
        m_page->AddSystem( m_system );
        m_doc->AddPage( m_page );
        pugi::xml_node current;
        for( current = mdiv.first_child( ); current; current = current.next_sibling( ) ) {
            ReadUnsupported( current );
        }
    }
    
    if ( !m_openTies.empty()) {
        std::vector<Note*>::iterator iter;
        for (iter = m_openTies.begin(); iter != m_openTies.end(); ++iter)
        {
            LogWarning("Terminal @tie for <note> '%s' could not be matched", (*iter)->GetUuid().c_str() );
        }
    }
    
    return true;
}

bool MeiInput::ReadMeiHeader( pugi::xml_node meiHead )
{
    return true;
}


bool MeiInput::ReadMeiPage( pugi::xml_node page )
{
    assert( m_page );
    
    if ( page.attribute( "page.height" ) ) {
        m_page->m_pageHeight = atoi ( page.attribute( "page.height" ).value() );
    }
    if ( page.attribute( "page.width" ) ) {
        m_page->m_pageWidth = atoi ( page.attribute( "page.width" ).value() );
    }
    if ( page.attribute( "page.leftmar" ) ) {
        m_page->m_pageLeftMar = atoi ( page.attribute( "page.leftmar" ).value() );
    }
    if ( page.attribute( "page.rightmar" ) ) {
        m_page->m_pageRightMar = atoi ( page.attribute( "page.rightmar" ).value() );
    }
    if ( page.attribute( "page.topmar" ) ) {
        m_page->m_pageTopMar = atoi ( page.attribute( "page.topmar" ).value() );
    }
    if ( page.attribute( "surface" ) ) {
        m_page->m_surface = page.attribute( "surface" ).value();
    }
    
    pugi::xml_node current;
    for( current = page.child( "system" ); current; current = current.next_sibling( "system" ) ) {
        m_system = new System( );
        SetMeiUuid( current, m_system );
        if (ReadMeiSystem( current )) {
            m_page->AddSystem( m_system );
        }
        else {
            delete m_system;
        }
        m_system = NULL;
    }
    // success only if at least one system was added to the page
    return (m_page->GetSystemCount() > 0);
}

bool MeiInput::ReadMeiSystem( pugi::xml_node system )
{
    assert( m_system );
    assert( !m_measure );
    assert( !m_staff );
    
    if ( system.attribute( "system.leftmar") ) {
        m_system->m_systemLeftMar = atoi ( system.attribute( "system.leftmar" ).value() );
    }
    if ( system.attribute( "system.rightmar" ) ) {
        m_system->m_systemRightMar = atoi ( system.attribute( "system.rightmar" ).value() );
    }
    if ( system.attribute( "uly" ) ) {
        m_system->m_yAbs = atoi ( system.attribute( "uly" ).value() );
    }
    
    pugi::xml_node current;
    
    
    // load the first scoreDef (if any) - temporary since we want to load all scoreDef elements
    if ( !m_hasScoreDef && ( current = system.first_child() ) && ( std::string( current.name() ) == "scoreDef") ) {
        LogDebug( "scoreDef" );
        m_scoreDef = &m_doc->m_scoreDef;
        SetMeiUuid( current, m_scoreDef );
        if (ReadMeiScoreDef( current )) {
            m_hasScoreDef = true;
        }
        else {
            m_hasScoreDef = false;
        }
    }
    
    // unmeasured music
    if ( system.child( "staff" ) ) {
        // this is the trick for un-measured music: we add one measure ( false )
        if ( !m_measure ) {
            m_measure = new Measure( false );
        }
        for( current = system.child( "staff" ); current; current = current.next_sibling( "staff" ) ) {
            m_staff = new Staff( );
            SetMeiUuid( current , m_staff );
            if ( ReadMeiStaff( current )) {
                m_measure->AddStaff( m_staff );
            }
            else {
                delete m_staff;
            }
            m_staff = NULL;
        }
        if ( m_measure->GetStaffCount() > 0) {
            m_system->AddMeasure( m_measure );
        }
        else {
            delete m_measure;
        }
        m_measure = NULL;
    }
    else {
        // measured
        for( current = system.child( "measure" ); current; current = current.next_sibling( "measure" ) ) {
            m_measure = new Measure( );
            SetMeiUuid( current, m_measure );
            if (ReadMeiMeasure( current )) {
                m_system->AddMeasure( m_measure );
            }
            else {
                delete m_measure;
            }
            m_measure = NULL;
        }
    }
    
    // success only if at least one measure was added to the system
    return (m_system->GetMeasureCount() > 0);
}

bool MeiInput::ReadMeiScoreDef( pugi::xml_node scoreDef )
{
    assert( m_scoreDef );
    assert( m_staffGrps.empty() );
    
    MeterSig meterSig;
    if ( ReadAttMeterSigLog( scoreDef, &meterSig, true ) ) {
        m_scoreDef->ReplaceMeterSig( &meterSig );
    }
    
    if ( scoreDef.attribute( "key.sig" ) ) {
        KeySig keysig(
                StrToKeySigNum( scoreDef.attribute( "key.sig" ).value() ),
                StrToKeySigType( scoreDef.attribute( "key.sig" ).value() ) );
        m_scoreDef->ReplaceKeySig( &keysig );
    }
    if ( scoreDef.attribute( "clef.line" ) && scoreDef.attribute( "clef.shape" ).value() ) {
        Clef clef;
        clef.m_clefId = StrToClef( scoreDef.attribute( "clef.shape" ).value() , scoreDef.attribute( "clef.line" ).value() );
        // this is obviously a short cut - assuming @clef.dis being SOLva
        if ( scoreDef.attribute( "clef.dis" ) ) {
            clef.m_clefId = SOLva;
        }
        m_scoreDef->ReplaceClef( &clef );
        // add other attributes for SOLva
    }
    
    pugi::xml_node current;
    for( current = scoreDef.child( "staffGrp" ); current; current = current.next_sibling( "staffGrp" ) ) {
        StaffGrp *staffGrp = new StaffGrp( );
        m_staffGrps.push_back( staffGrp );
        SetMeiUuid( current , staffGrp );
        if (ReadMeiStaffGrp( current )) {
            m_scoreDef->AddStaffGrp( staffGrp );
        }
        else {
            delete staffGrp;
        }
        m_staffGrps.pop_back();
    }
    
    return true;
}

bool MeiInput::ReadMeiStaffGrp( pugi::xml_node staffGrp )
{
    assert( !m_staffGrps.empty() );
    assert( !m_staffDef );
    
    StaffGrp *currentStaffGrp = m_staffGrps.back();
    
    if ( staffGrp.attribute( "symbol" ) ) {
        currentStaffGrp->SetSymbol( StrToStaffGrpSymbol( staffGrp.attribute( "symbol" ).value() ) );
    }
    if ( staffGrp.attribute( "barthru" ) ) {
        currentStaffGrp->SetBarthru( StrToBool( staffGrp.attribute( "barthru" ).value() ) );
    }
    
    pugi::xml_node current;
    for( current = staffGrp.first_child( ); current; current = current.next_sibling( ) ) {
        if ( std::string( current.name() ) == "staffGrp" ) {
            StaffGrp *staffGrp = new StaffGrp( );
            m_staffGrps.push_back( staffGrp );
            SetMeiUuid( current , staffGrp );
            if (ReadMeiStaffGrp( current )) {
                currentStaffGrp->AddStaffGrp( staffGrp );
            }
            else {
                delete staffGrp;
            }
            m_staffGrps.pop_back();            
        }
        else if ( std::string( current.name() ) == "staffDef" ) {
            m_staffDef = new StaffDef( );
            SetMeiUuid( current , m_staffDef );
            if (ReadMeiStaffDef( current )) {
                currentStaffGrp->AddStaffDef( m_staffDef );
            }
            else {
                delete m_staffDef;
            }
            m_staffDef = NULL;
        }        
    }
    
    return true;
}

bool MeiInput::ReadMeiStaffDef( pugi::xml_node staffDef )
{
    assert( m_staffDef );
    
    if ( staffDef.attribute( "n" ) ) {
        m_staffDef->SetStaffNo( atoi ( staffDef.attribute( "n" ).value() ) );
    }
    else {
        LogWarning("No @n on <staffDef>");
    }
    if ( staffDef.attribute( "key.sig" ) ) {
        KeySig keysig(
                         StrToKeySigNum( staffDef.attribute( "key.sig" ).value() ),
                         StrToKeySigType( staffDef.attribute( "key.sig" ).value() ) );
        m_staffDef->ReplaceKeySig( &keysig );
    }
    if ( staffDef.attribute( "clef.line" ) && staffDef.attribute( "clef.shape" ) ) {
        Clef clef;
        clef.m_clefId = StrToClef( staffDef.attribute( "clef.shape" ).value() , staffDef.attribute( "clef.line" ).value() );
        // this is obviously a short cut - assuming @clef.dis being SOLva
        if ( staffDef.attribute( "clef.dis" ) ) {
            clef.m_clefId = SOLva;
        }
        m_staffDef->ReplaceClef( &clef );
    }
    
    return true;
}

bool MeiInput::ReadMeiMeasure( pugi::xml_node measure )
{
    assert( m_measure );
    assert( !m_staff );
    
    ReadAttCommon( measure, m_measure );
    if ( measure.attribute( "right" ) ) {
        m_measure->GetRightBarline()->m_barlineType = StrToBarlineType( measure.attribute( "right" ).value() );
    }
    
    pugi::xml_node current;
    for( current = measure.first_child( ); current; current = current.next_sibling( ) ) {
        if ( std::string( current.name() ) == "staff" ) {
            m_staff = new Staff( );
            SetMeiUuid( current , m_staff );
            if ( ReadMeiStaff( current )) {
                m_measure->AddStaff( m_staff );
            }
            else {
                delete m_staff;
            }
            m_staff = NULL;
        }
        else ReadUnsupported( current );
    }
    // success only if at least one staff was added to the measure
    return (m_measure->GetStaffCount() > 0);
}

bool MeiInput::ReadMeiStaff( pugi::xml_node staff )
{
    assert( m_staff );
    assert( !m_layer );
    
    if ( staff.attribute( "n" ) ) {
        m_staff->SetStaffNo( atoi ( staff.attribute( "n" ).value() ) );
    }
    else {
        LogWarning("No @n on <staff>");
    }
    if ( staff.attribute( "uly" ) ) {
        m_staff->m_yAbs = atoi ( staff.attribute( "uly" ).value() );
    }
    if ( staff.attribute( "label" ) ) {
        // we use type only for typing mensural notation
        m_staff->notAnc = true;
    }
    
    pugi::xml_node current;
    for( current = staff.child( "layer" ); current; current = current.next_sibling( "layer" ) ) {
        m_layer = new Layer( 1 );
        m_currentLayer = m_layer;
        SetMeiUuid( current , m_layer );
        if (ReadMeiLayer( current )) {
            m_staff->AddLayer( m_layer );
        }
        else {
            delete m_layer;
        }
        m_layer = NULL;
    }
    
    // success only if at least one measure was added to the staff
    return (m_staff->GetLayerCount() > 0);
}

bool MeiInput::ReadMeiLayer( pugi::xml_node layer )
{
    assert( m_layer );
    
    if ( layer.attribute( "n" ) ) {
        m_layer->SetLayerNo( atoi ( layer.attribute( "n" ).value() ) );
    }
    else {
        LogWarning("No @n on <layer>");
    }
    
    pugi::xml_node current;
    for( current = layer.first_child( ); current; current = current.next_sibling( ) ) {
        ReadMeiLayerElement( current );
    }
    // success in any case
    return true;
}

bool MeiInput::ReadMeiLayerElement( pugi::xml_node xmlElement )
{
    LayerElement *vrvElement = NULL;
    if ( std::string( xmlElement.name() )  == "barLine" ) {
        vrvElement = ReadMeiBarline( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "beam" ) {
        vrvElement = ReadMeiBeam( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "clef" ) {
        vrvElement = ReadMeiClef( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "mensur" ) {
        vrvElement = ReadMeiMensur( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "meterSig" ) {
        vrvElement = ReadMeiMeterSig( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "note" ) {
        vrvElement = ReadMeiNote( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "rest" ) {
        vrvElement = ReadMeiRest( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "mRest" ) {
        vrvElement = ReadMeiMRest( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "multiRest" ) {
        vrvElement = ReadMeiMultiRest( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "tuplet" ) {
        vrvElement = ReadMeiTuplet( xmlElement );
    }
    // symbols
    else if ( std::string( xmlElement.name() ) == "accid" ) {
        vrvElement = ReadMeiAccid( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "custos" ) {
        vrvElement = ReadMeiCustos( xmlElement );
    }
    else if ( std::string( xmlElement.name() ) == "dot" ) {
        vrvElement = ReadMeiDot( xmlElement );
    }
    // app
    else if ( std::string( xmlElement.name() ) == "app" ) {
        vrvElement = ReadMeiApp( xmlElement );
    }
    // unkown            
    else {
        LogDebug("Element %s ignored", xmlElement.name() );
    }
    
    if ( !vrvElement ) {
        return false;
    }
    
    if ( xmlElement.attribute( "ulx" ) ) {
        vrvElement->m_xAbs = atoi ( xmlElement.attribute( "ulx" ).value() );
    }
    ReadSameAsAttr( xmlElement, vrvElement );
    SetMeiUuid( xmlElement, vrvElement );
    
    AddLayerElement( vrvElement );
    return true;
}

LayerElement *MeiInput::ReadMeiBarline( pugi::xml_node barline )
{
    Barline *vrvBarline = new Barline();
    
    return vrvBarline;    
}

LayerElement *MeiInput::ReadMeiBeam( pugi::xml_node beam )
{
    assert ( !m_beam );
    
    // m_beam will be used for adding elements to the beam
    m_beam = new Beam();
    
    Object *previousLayer = m_currentLayer;
    m_currentLayer = m_beam;
    
    pugi::xml_node current;
    for( current = beam.first_child( ); current; current = current.next_sibling( ) ) {
        ReadMeiLayerElement( current );
    }
    
    if ( m_beam->GetNoteCount() == 1 ) {
        LogWarning("<beam> with only one note");
    }
    // switch back to the previous one
    m_currentLayer = previousLayer;
    if ( m_beam->GetNoteCount() < 1 ) {
        delete m_beam;
        m_beam = NULL;
        return NULL;
    } 
    else {
        // set the member to NULL but keep a pointer to be returned        
        Beam *vrvBeam = m_beam;
        m_beam = NULL;
        return vrvBeam;
    }
}

LayerElement *MeiInput::ReadMeiClef( pugi::xml_node clef )
{ 
    Clef *vrvClef = new Clef(); 
    if ( clef.attribute( "shape" ) && clef.attribute( "line" ) ) {
        vrvClef->m_clefId = StrToClef( clef.attribute( "shape" ).value() , clef.attribute( "line" ).value() );
    }
    
    return vrvClef;
}


LayerElement *MeiInput::ReadMeiMensur( pugi::xml_node mensur )
{
    Mensur *vrvMensur = new Mensur();
    
    if ( mensur.attribute( "sign" ) ) {
        vrvMensur->m_sign = StrToMensurationSign( mensur.attribute( "sign" ).value() );
    }
    if ( mensur.attribute( "dot" ) ) {
        vrvMensur->m_dot = ( strcmp( mensur.attribute( "dot" ).value(), "true" ) == 0 );
    }
    if ( mensur.attribute( "slash" ) ) {
        vrvMensur->m_slash =  1; //atoi( mensur.attribute( "Slash" ) );
    }
    if ( mensur.attribute( "orient" ) ) {
        vrvMensur->m_reversed = ( strcmp ( mensur.attribute( "orient" ).value(), "reversed" ) == 0 );
    }
    if ( mensur.attribute( "num" ) ) {
        vrvMensur->m_num = atoi ( mensur.attribute( "num" ).value() );
    }
    if ( mensur.attribute( "numbase" ) ) {
        vrvMensur->m_numBase = atoi ( mensur.attribute( "numbase" ).value() );
    }
    // missing m_meterSymb
    
    return vrvMensur;
}
    
LayerElement *MeiInput::ReadMeiMeterSig( pugi::xml_node meterSig )
{
    MeterSig *vrvMeterSig = new MeterSig();
    ReadAttMeterSigLog(meterSig, vrvMeterSig);
    return vrvMeterSig;
}
    
LayerElement *MeiInput::ReadMeiMRest( pugi::xml_node mRest )
{
    MRest *vrvMRest = new MRest();    
    return vrvMRest;
}

LayerElement *MeiInput::ReadMeiMultiRest( pugi::xml_node multiRest )
{
	MultiRest *vrvMultiRest = new MultiRest( 1 );
    
	// pitch
    if ( multiRest.attribute( "num" ) ) {
        vrvMultiRest->SetNumber( atoi ( multiRest.attribute( "num" ).value() ) );
    }
	
	return vrvMultiRest;
}

LayerElement *MeiInput::ReadMeiNote( pugi::xml_node note )
{
	Note *vrvNote = new Note();
    
	// pitch
	if ( note.attribute( "pname" ) ) {
		vrvNote->m_pname = StrToPitch( note.attribute( "pname" ).value() );
	}
	// oct
	if ( note.attribute( "oct" ) ) {
		vrvNote->m_oct = StrToOct( note.attribute( "oct" ).value() );
	}
	// duration
	if ( note.attribute( "dur" ) ) {
		vrvNote->m_dur = StrToDur( note.attribute( "dur" ).value() );
	}
    // duration gestural
	if ( note.attribute( "dur.ges" ) ) {
		vrvNote->m_durGes = StrToDur( note.attribute( "dur.ges" ).value() );
	}
    // dots
    if ( note.attribute( "dots" ) ) {
		vrvNote->m_dots = atoi( note.attribute( "dots" ).value() );
	}
    // accid
    if ( note.attribute( "accid" ) ) {
		vrvNote->m_accid = StrToAccid( note.attribute( "accid" ).value() );
	}
    // grace
    if ( note.attribute( "grace" ) ) {
		vrvNote->m_cueSize = true; //
	}
    // ligature
    if ( note.attribute( "lig" ) ) {
        vrvNote->m_lig = true; // this has to be double checked
        if ( strcmp( note.attribute( "lig" ).value(), "obliqua" ) == 0 ) {
            vrvNote->m_ligObliqua = true;
        }
    }
    // stem direction
    if ( note.attribute( "stem.dir" ) ) {
        // we use it to indicate opposite direction
        //vrvNote->m_stemDir = 1;
    }
    // coloration
    if ( note.attribute( "colored" ) ) {
        vrvNote->m_colored = ( strcmp ( note.attribute( "colored" ).value(), "true" ) == 0 );
    }
    // coloration
    if ( note.attribute( "tie" ) ) {
        if ( (strcmp ( note.attribute( "tie" ).value(), "i" ) == 0) || (strcmp ( note.attribute( "tie" ).value(), "m" ) == 0) ) {
            vrvNote->SetTieAttrInitial();
            m_openTies.push_back( vrvNote );
        }
        if ( (strcmp ( note.attribute( "tie" ).value(), "t" ) == 0) || (strcmp ( note.attribute( "tie" ).value(), "m" ) == 0) ) {
            if (!FindOpenTie( vrvNote ) ) {
                LogWarning("Initial @tie not found" );
            }
        }
    }
	
	return vrvNote;
}


LayerElement *MeiInput::ReadMeiRest( pugi::xml_node rest )
{
    Rest *vrvRest = new Rest();
    
	// duration
	if ( rest.attribute( "dur" ) ) {
		vrvRest->m_dur = StrToDur( rest.attribute( "dur" ).value() );
	}
    if ( rest.attribute( "dots" ) ) {
		vrvRest->m_dots = atoi( rest.attribute( "dots" ).value() );
	}
    // position
	if ( rest.attribute( "ploc" ) ) {
		vrvRest->m_pname = StrToPitch( rest.attribute( "ploc" ).value() );
	}
	// oct
	if ( rest.attribute( "oloc" ) ) {
		vrvRest->m_oct = StrToOct( rest.attribute( "oloc" ).value() );
	}
	
    return vrvRest;
}


LayerElement *MeiInput::ReadMeiTuplet( pugi::xml_node tuplet )
{
    assert ( !m_tuplet );
    
    // m_tuplet will be used for adding elements to the beam
    m_tuplet = new Tuplet();
    
    Object *previousLayer = m_currentLayer;
    m_currentLayer = m_tuplet;
    
    // Read in the numerator and denominator properties
    if ( tuplet.attribute( "num" ) ) {
		m_tuplet->SetNum(atoi( tuplet.attribute( "num" ).value() ));
	}
    if ( tuplet.attribute( "numbase" ) ) {
		m_tuplet->SetNumBase(atoi( tuplet.attribute( "numbase" ).value() ));
	}
    
    pugi::xml_node current;
    for( current = tuplet.first_child( ); current; current = current.next_sibling( ) ) {
        ReadMeiLayerElement( current );
    }
    
    if ( m_tuplet->GetNoteCount() == 1 ) {
        LogWarning("<tuplet> with only one note");
    }
    // switch back to the previous one
    m_currentLayer = previousLayer;
    if ( m_tuplet->GetNoteCount() < 1 ) {
        delete m_tuplet;
        return NULL;
    }
    else {
        // set the member to NULL but keep a pointer to be returned
        Tuplet *vrvTuplet = m_tuplet;
        m_tuplet = NULL;
        return vrvTuplet;
    }
}


LayerElement *MeiInput::ReadMeiAccid( pugi::xml_node accid )
{
    Symbol *vrvAccid = new Symbol( SYMBOL_ACCID );
    
    if ( accid.attribute( "accid" ) ) {
        vrvAccid->m_accid = StrToAccid( accid.attribute( "accid" ).value() );
    }
    // position
	if ( accid.attribute( "ploc" ) ) {
		vrvAccid->m_pname = StrToPitch( accid.attribute( "ploc" ).value() );
	}
	// oct
	if ( accid.attribute( "oloc" ) ) {
		vrvAccid->m_oct = StrToOct( accid.attribute( "oloc" ).value() );
	}
	
	return vrvAccid;
}

LayerElement *MeiInput::ReadMeiCustos( pugi::xml_node custos )
{
    Symbol *vrvCustos = new Symbol( SYMBOL_CUSTOS );
    
	// position (pitch)
	if ( custos.attribute( "pname" ) ) {
		vrvCustos->m_pname = StrToPitch( custos.attribute( "pname" ).value() );
	}
	// oct
	if ( custos.attribute( "oct" ) ) {
		vrvCustos->m_oct = StrToOct( custos.attribute( "oct" ).value() );
	}
	
	return vrvCustos;    
}

LayerElement *MeiInput::ReadMeiDot( pugi::xml_node dot )
{
    Symbol *vrvDot = new Symbol( SYMBOL_DOT );
    
    vrvDot->m_dot = 0;
    // missing m_dots
    // position
	if ( dot.attribute( "ploc" ) ) {
		vrvDot->m_pname = StrToPitch( dot.attribute( "ploc" ).value() );
	}
	// oct
	if ( dot.attribute( "oloc" ) ) {
		vrvDot->m_oct = StrToOct( dot.attribute( "oloc" ).value() );
	}
	
	return vrvDot;
}

LayerElement *MeiInput::ReadMeiApp( pugi::xml_node app )
{
    m_layerApp = new LayerApp( );
   
    pugi::xml_node current;
    for( current = app.child( "rdg" ); current; current = current.next_sibling( "rdg" ) ) {
        ReadMeiRdg( current );
	}
	
    // set the member to NULL but keep a pointer to be returned
    LayerApp *layerApp = m_layerApp;
    m_layerApp = NULL;
    
    return layerApp;
}

bool MeiInput::ReadMeiRdg( pugi::xml_node rdg )
{
    assert ( !m_layerRdg );
    assert( m_layerApp );
    
    m_layerRdg = new LayerRdg( );
    
    if ( rdg.attribute( "source" ) ) {
        m_layerRdg->m_source = rdg.attribute( "source" ).value();
    }
    
    // switch to the rdg
    Object *previousLayer = m_currentLayer;
    m_currentLayer = m_layerRdg;
 
    pugi::xml_node current;
    for( current = rdg.first_child( ); current; current = current.next_sibling( ) ) {
        ReadMeiLayerElement( current );
    }
    
    // switch back to the previous one
    m_currentLayer = previousLayer;

    // set the member to NULL but keep a pointer to be returned
    LayerRdg *layerRdg = m_layerRdg;
    m_layerRdg = NULL;
    
    return layerRdg;
}


void MeiInput::ReadSameAsAttr( pugi::xml_node element, Object *object )
{
    if ( !element.attribute( "sameas" ) ) {
        return;
    }
    
    object->m_sameAs = element.attribute( "sameas" ).value();
}

    
void MeiInput::ReadAttCommon( pugi::xml_node element, Object *object )
{
    AttCommon *att = dynamic_cast<AttCommon*>( object );
    assert( att );
    if ( element.attribute( "label" ) ) {
        att->SetLabel( element.attribute( "label" ).value() );
    }
    if ( element.attribute( "n" ) ) {
        att->SetN( atoi( element.attribute( "n" ).value() ) );
    }
}
    
bool MeiInput::ReadAttMeterSigLog( pugi::xml_node element, Object *object, bool isMeterSigLogDefault )
{
    std::string prefix = "";
    bool hasAttribute = false;
    if ( isMeterSigLogDefault ) {
        prefix = "meter.";
    }
    AttMeterSigLog *att = dynamic_cast<AttMeterSigLog*>( object );
    assert( att );
    if ( element.attribute( (prefix + "count").c_str() ) ) {
        att->SetCount( atoi( element.attribute( (prefix + "count").c_str() ).value() ) );
        hasAttribute = true;
    }
    if ( element.attribute( (prefix + "sym").c_str() ) ) {
        att->SetSym( StrToMeterSign( element.attribute( (prefix + "sym").c_str() ).value() ) );
        hasAttribute = true;
    }
    if ( element.attribute( (prefix + "unit").c_str() ) ) {
        att->SetUnit( atoi( element.attribute( (prefix + "unit").c_str() ).value() ) );
        hasAttribute = true;
    }
    return hasAttribute;
}

void MeiInput::AddLayerElement( LayerElement *element )
{
    assert( m_currentLayer );
    if ( dynamic_cast<Layer*>( m_currentLayer ) ) {
        ((Layer*)m_currentLayer)->AddElement( element );
    }
    else if ( dynamic_cast<LayerRdg*>( m_currentLayer ) ) {
        ((LayerRdg*)m_currentLayer)->AddElement( element );
    }
    else if ( dynamic_cast<Beam*>( m_currentLayer ) ) {
        ((Beam*)m_currentLayer)->AddElement( element );
    }
    else if ( dynamic_cast<Tuplet*>( m_currentLayer ) ) {
        ((Tuplet*)m_currentLayer)->AddElement( element );
    }
    
}


bool MeiInput::ReadUnsupported( pugi::xml_node element )
{
    if ( std::string( element.name() ) == "score" ) {
        pugi::xml_node current;
        for( current = element.first_child( ); current; current = current.next_sibling( ) ) {
            ReadUnsupported( current );
        }
    }
    if ( std::string( element.name() ) == "section" ) {
        pugi::xml_node current;
        for( current = element.first_child( ); current; current = current.next_sibling( ) ) {
            ReadUnsupported( current );
        }       
    }
    else if ( std::string( element.name() ) == "measure" ) {
        LogDebug( "measure" );
        m_measure = new Measure( );
        SetMeiUuid( element, m_measure );
        if (ReadMeiMeasure( element )) {
            m_system->AddMeasure( m_measure );
        }
        else {
            delete m_measure;
        }
        m_measure = NULL;
    }
    else if ( std::string( element.name() ) == "tupletSpan" ) {
        if (!ReadTupletSpanAsTuplet( element )) {
            LogWarning( "<tupletSpan> not readable as <tuplet> and ignored" );
        }
    }
    else if ( std::string( element.name() ) == "slur" ) {
        if (!ReadSlurAsSlurAttr( element )) {
            LogWarning( "<slur> not readable as @slur and ignored" );
        }
    }
    /*
    else if ( std::string( element.name() ) == "staff" ) {
        LogDebug( "staff" );
        int n = 1;
        if ( element.attribute( "n" ) ) {
            element.attribute( "n", &n );
        }
        Staff *staff = m_system->GetStaff( n - 1 );
        if ( staff ) {
            m_staff = staff;
        }
        else
        {
            m_staff = new Staff( n );
            m_system->AddStaff( m_staff );
        }
        m_measure = new Measure( *m_contentBasedMeasure );
        ReadMeiStaff( element );
    }
    */
    else if ( (std::string( element.name() ) == "pb")
             && (m_system->GetMeasureCount() > 0 )  && !m_ignoreLayoutInformation) {
        LogDebug( "pb" );
        this->m_hasLayoutInformation = true;
        m_page = new Page( );
        m_system = new System( );
        m_page->AddSystem( m_system );
        m_doc->AddPage( m_page );
        
    }
    else if ( (std::string( element.name() ) == "sb")
             && (m_page->GetSystemCount() > 0 )  && !m_ignoreLayoutInformation) {
        LogDebug( "sb" );
        this->m_hasLayoutInformation = true;
        m_system = new System( );
        m_page->AddSystem( m_system );
    }
    else if ( (std::string( element.name() ) == "scoreDef") && ( !m_hasScoreDef ) ) {
        LogDebug( "scoreDef" );
        m_scoreDef = &m_doc->m_scoreDef;
        SetMeiUuid( element, m_scoreDef );
        if (ReadMeiScoreDef( element )) {
            m_hasScoreDef = true;
        }
        else {
            m_hasScoreDef = false;
        }
    }
    else {
        LogWarning( "Elements <%s> ignored", element.name() );
    }
    return true;
}
    
bool MeiInput::ReadSlurAsSlurAttr(pugi::xml_node slur)
{
    assert( m_measure );
    
    LayerElement *start = NULL;
    LayerElement *end = NULL;
    
	// position (pitch)
	if ( slur.attribute( "startid" ) ) {
        std::string refId = ExtractUuidFragment( slur.attribute( "startid" ).value() );
        start = dynamic_cast<LayerElement*>( m_measure->FindChildByUuid( refId ) );

        if (!start || !start->IsNote()) {
            LogWarning( "Note with @startid '%s' not found when trying to read the <slur>", refId.c_str() );
        }    
        
	}
	if ( slur.attribute( "endid" ) ) {
        std::string refId = ExtractUuidFragment( slur.attribute( "endid" ).value() );
        end = dynamic_cast<LayerElement*>( m_measure->FindChildByUuid( refId ) );
        
        if (!end || !end->IsNote()) {
            LogWarning( "Note with @endid '%s' not found when trying to read the <slur>", refId.c_str() );
        }
	}
    Note *startNote = dynamic_cast<Note*>(start);
    Note *endNote = dynamic_cast<Note*>(end);
    
    if (!start || !end || !startNote || !endNote) {
        return false;
    }
    
    startNote->SetSlurAttrInitial();
    endNote->SetSlurAttrTerminal( startNote );
    return true;

}
    
bool MeiInput::ReadTupletSpanAsTuplet(pugi::xml_node tupletSpan)
{
    assert( m_measure );
    
    Tuplet *tuplet = new Tuplet();
    SetMeiUuid(tupletSpan, tuplet);
    
    LayerElement *start = NULL;
    LayerElement *end = NULL;
    
    // Read in the numerator and denominator properties
    if ( tupletSpan.attribute( "num" ) ) {
        tuplet->SetNum(atoi( tupletSpan.attribute( "num" ).value() ));
    }
    if ( tupletSpan.attribute( "numbase" ) ) {
        tuplet->SetNumBase(atoi( tupletSpan.attribute( "numbase" ).value() ));
    }
    
    // position (pitch)
    if ( tupletSpan.attribute( "startid" ) ) {
        std::string refId = ExtractUuidFragment( tupletSpan.attribute( "startid" ).value() );
        start = dynamic_cast<LayerElement*>( m_measure->FindChildByUuid( refId ) );
        
        if (!start) {
            LogWarning( "Element with @startid '%s' not found when trying to read the <tupletSpan>", refId.c_str() );
        }
        
    }
    if ( tupletSpan.attribute( "endid" ) ) {
        std::string refId = ExtractUuidFragment( tupletSpan.attribute( "endid" ).value() );
        end = dynamic_cast<LayerElement*>( m_measure->FindChildByUuid( refId ) );
        
        if (!end) {
            LogWarning( "Element with @endid '%s' not found when trying to read the <tupletSpan>", refId.c_str() );
        }
    }
    if (!start || !end) {
        delete tuplet;
        return false;
    }
    
    LayerElement *startChild =  dynamic_cast<LayerElement*>( start->GetLastParentNot( &typeid(Layer) ) );
    LayerElement *endChild =  dynamic_cast<LayerElement*>( end->GetLastParentNot( &typeid(Layer) ) );
    
    if ( !startChild || !endChild || (startChild->m_parent != endChild->m_parent) ) {
        LogWarning( "Start and end elements for <tupletSpan> '%s' not in the same layer", tuplet->GetUuid().c_str() );
        delete tuplet;
        return false;
    }
    
    Layer *parentLayer = dynamic_cast<Layer*>( startChild->m_parent );
    
    int startIdx = startChild->GetIdx();
    int endIdx = endChild->GetIdx();
    LogDebug("%d %d %s!", startIdx, endIdx, start->GetUuid().c_str());
    int i;
    for (i = endIdx; i >= startIdx; i--) {
        tuplet->AddElement( dynamic_cast<LayerElement*>( parentLayer->DetachChild(i) ) );
    }
    parentLayer->InsertChild( tuplet, startIdx );
    return true;
    
}

bool MeiInput::FindOpenTie( Note *terminalNote )
{
    assert( m_staff );
    assert( m_layer );
    
    std::vector<Note*>::iterator iter;
    for (iter = m_openTies.begin(); iter != m_openTies.end(); ++iter)
    {
        // we need to get the parent layer and the parent staff for comparing their number
        Layer *parentLayer = dynamic_cast<Layer*>( (*iter)->GetFirstParent( &typeid(Layer) ) );
        Staff *parentStaff = dynamic_cast<Staff*>( (*iter)->GetFirstParent( &typeid(Staff) ) );
        // We assume that if the note has no parent layer or no parent staff it,
        // is because we are in the same staff or layer (e.g., beam) and they have not been added
        //to their parent (staff or layer) yet.
        // If we have one, compare the number
        if ( (parentStaff) && (m_staff->GetStaffNo() != parentStaff->GetStaffNo()) ) {
            continue;
        }
        // same layer?
        if ( (parentLayer) && (m_layer->GetLayerNo() != parentLayer->GetLayerNo()) ) {
            continue;
        }
        // we only compare oct and pname because alteration is not relevant for ties
        if ( (terminalNote->m_oct == (*iter)->m_oct) && (terminalNote->m_pname == (*iter)->m_pname) ) {
            terminalNote->SetTieAttrTerminal( *iter );
            m_openTies.erase(iter);
            return true;
        }
        
    }
    return false;
}

void MeiInput::SetMeiUuid( pugi::xml_node element, Object *object )
{
    if ( !element.attribute( "xml:id" ) ) {
        return;
    }
    
    object->SetUuid( element.attribute( "xml:id" ).value() );
}

bool MeiInput::StrToBool(std::string value)
{
    if (value == "false") return false;
	return true;
}

int MeiInput::StrToDur(std::string dur)
{
    int value;
    if (dur == "longa") value = DUR_LG;
    else if (dur == "brevis") value = DUR_BR;
    else if (dur == "semibrevis") value = DUR_1;
    else if (dur == "minima") value = DUR_2;
    else if (dur == "semiminima") value = DUR_4;
    else if (dur == "fusa") value = DUR_8;
    else if (dur == "semifusa") value = DUR_16;
    else if (dur == "long") value = DUR_LG;
    else if (dur == "breve") value = DUR_BR;
    else if (dur == "1") value = DUR_1;
    else if (dur == "2") value = DUR_2;
    else if (dur == "4") value = DUR_4;
    else if (dur == "8") value = DUR_8;
    else if (dur == "16") value = DUR_16;
    else if (dur == "32") value = DUR_32;
    else if (dur == "64") value = DUR_64;
    else if (dur == "128") value = DUR_128;
	else {
        if ((dur.length() > 0) && (dur[dur.length()-1] == 'p')) {
            LogWarning("PPQ duration values are not supported");
        }
        else {
            LogWarning("Unknown @dur value '%s'", dur.c_str());
        }
        value = VRV_UNSET;
	}
    return value;
}

int MeiInput::StrToOct(std::string oct)
{
	return atoi(oct.c_str());
}

int MeiInput::StrToPitch(std::string pitch)
{
    int value;
    if (pitch == "c") value = PITCH_C;
    else if (pitch == "d") value = PITCH_D;
    else if (pitch == "e") value = PITCH_E;
    else if (pitch == "f") value = PITCH_F;
    else if (pitch == "g") value = PITCH_G;
    else if (pitch == "a") value = PITCH_A;
    else if (pitch == "b") value = PITCH_B;
    else {
		LogWarning("Unknow @pname value '%s'", pitch.c_str());
        value = PITCH_C;
    }
    return value;
}


unsigned char MeiInput::StrToAccid(std::string accid)
{
    unsigned char value;
    if ( accid == "s" ) value = ACCID_SHARP;
    else if ( accid == "f" ) value = ACCID_FLAT;
    else if ( accid == "n" ) value = ACCID_NATURAL;
    else if ( accid == "x" ) value = ACCID_DOUBLE_SHARP;
    else if ( accid == "ff" ) value = ACCID_DOUBLE_FLAT;
    else if ( accid == "ns" ) value = ACCID_QUARTER_SHARP;
    else if ( accid == "nf" ) value = ACCID_QUARTER_FLAT;
    else {
        LogWarning("Unknown accid '%s'", accid.c_str() );
        value = ACCID_NATURAL;
    }
	return value;
}


ClefId MeiInput::StrToClef( std::string shape, std::string line )
{
    ClefId clefId = SOL2;
    std::string clef = shape + line;
    if ( clef == "G2" ) clefId = SOL2;
    else if ( clef == "G1" ) clefId = SOL1; 
    else if ( clef == "F5" ) clefId = FA5;
    else if ( clef == "F4" ) clefId = FA4; 
    else if ( clef == "F3" ) clefId = FA3; 
    else if ( clef == "C1" ) clefId = UT1; 
    else if ( clef == "C2" ) clefId = UT2; 
    else if ( clef == "C3" ) clefId = UT3; 
    else if ( clef == "C4" ) clefId = UT4; 
    else if ( clef == "C5" ) clefId = UT5; 
    else 
    {
        LogWarning("Unsupported clef with @shape '%s' and @line '%s'", shape.c_str(), line.c_str() );
    }
    return clefId;
}

MensurationSign MeiInput::StrToMensurationSign(std::string sign)
{
    if (sign == "C") return MENSURATIONSIGN_C;
    else if (sign == "O") return MENSURATIONSIGN_O;
    else {
        LogWarning("Unsupported mensur sign '%s'", sign.c_str() );
	}
    // default
	return MENSURATIONSIGN_C;
}
    
MeterSign MeiInput::StrToMeterSign(std::string sign)
{
    if (sign == "common") return METERSIGN_COMMON;
    else if (sign == "cut") return METERSIGN_CUT;
    else {
        LogWarning("Unsupported meter sign '%s'", sign.c_str() );
    }
    // default
    return METERSIGN_NONE;
}

DocType MeiInput::StrToDocType(std::string type)
{
    if (type == "raw") return Raw;
    else if (type == "rendering") return Rendering;
    else if (type == "transcription") return Transcription;
    else {
        LogWarning("Unknown layout type '%s'", type.c_str() );
	}
    // default
	return Raw;
}

unsigned char MeiInput::StrToKeySigType(std::string accid)
{
    if ( accid == "0" ) return  ACCID_NATURAL;
    else if ( accid.at(1) == 'f' ) return ACCID_FLAT;
    else if ( accid.at(1) == 's' ) return ACCID_SHARP;
    else {
        LogWarning("Unknown keysig '%s'", accid.c_str() );
        return ACCID_NATURAL;
    }
}

int MeiInput::StrToKeySigNum(std::string accid)
{
    if ( accid == "0" ) return  0;
    else {
        // low level way, remove '0', which is 48
        return accid.at(0) - '0';
    }
}

BarlineType MeiInput::StrToBarlineType(std::string type)
{
    if (type == "single") return BARLINE_SINGLE;
    else if (type == "end") return BARLINE_END;
    else if (type == "dbl") return BARLINE_DBL;
    else if (type == "rptend") return BARLINE_RPTEND;
    else if (type == "rptstart") return BARLINE_RPTSTART;
    else if (type == "rptboth") return BARLINE_RPTBOTH;
    else {
        LogWarning("Unknown barline type '%s'", type.c_str() );
	}
    // default
	return BARLINE_SINGLE;
}

StaffGrpSymbol MeiInput::StrToStaffGrpSymbol(std::string symbol)
{
    if (symbol == "line") return STAFFGRP_LINE;
    else if (symbol == "brace") return STAFFGRP_BRACE;
    else if (symbol == "bracket") return STAFFGRP_BRACKET;
    else {
        LogWarning("Unknown staffGrp @symbol '%s'", symbol.c_str() );
	}
    // default
	return STAFFGRP_LINE;
}
    
std::string MeiInput::ExtractUuidFragment(std::string refUuid)
{
    unsigned pos = refUuid.find_last_of("#");
    if ( (pos != std::string::npos) && (pos < refUuid.length() - 1) ) {
        refUuid = refUuid.substr( pos + 1 );
    }
    return refUuid;
}

} // namespace vrv

