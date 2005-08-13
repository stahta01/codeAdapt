#include "wxswindowres.h"

#include "../wxswidgetfactory.h"
#include "../wxswindoweditor.h"
#include "../wxscodegen.h"
#include <manager.h>
#include <wx/xrc/xmlres.h>
#include <editormanager.h>

const wxChar* EmptySource =
_T("\
#include \"$(Include)\"\n\
\n\
BEGIN_EVENT_TABLE($(ClassName),$(BaseClassName))\n\
//(*EventTable($(ClassName))\n\
//*)\n\
END_EVENT_TABLE()\n\
\n\
$(ClassName)::$(ClassName)(wxWindow* parent,wxWindowID id):\n\
    $(BaseClassCtor)\n\
{\n\
    //(*Initialize($(ClassName))\n\
    //*)\n\
}\n\
\n\
$(ClassName)::~$(ClassName)()\n\
{\n\
}\n\
\n\
");

const wxChar* EmptyHeader =
_T("\
#ifndef $(Guard)\n\
#define $(Guard)\n\
\n\
//(*Headers($(ClassName))\n\
#include <wx/wx.h>\n\
//*)\n\
\n\
class $(ClassName): public $(BaseClassName)\n\
{\n\
    public:\n\
\n\
        $(ClassName)(wxWindow* parent,wxWindowID id = -1);\n\
        virtual ~$(ClassName)();\n\
\n\
        //(*Identifiers($(ClassName))\n\
        //*)\n\
\n\
    protected:\n\
\n\
        //(*Handlers($(ClassName))\n\
        //*)\n\
\n\
        //(*Declarations($(ClassName))\n\
        //*)\n\
\n\
    private:\n\
\n\
        DECLARE_EVENT_TABLE()\n\
};\n\
\n\
#endif\n\
");


wxsWindowRes::wxsWindowRes(
    wxsProject* Project,
    const wxString& Class, 
    const wxString& Xrc, 
    const wxString& Src,
    const wxString& Head,
    WindowResType _Type):
        wxsResource(Project),
        ClassName(Class),
        XrcFile(Xrc),
        SrcFile(Src),
        HFile(Head),
        Type(_Type)
{
    RootWidget = wxsWidgetFactory::Get()->Generate(GetWidgetClass(true),this);
}

wxsWindowRes::~wxsWindowRes()
{
    EditClose();
    wxsWidgetFactory::Get()->Kill(RootWidget);
    switch ( Type )
    {
    	case Dialog:
            GetProject()->DeleteDialog(dynamic_cast<wxsDialogRes*>(this));
            break;
            
        case Frame:
            GetProject()->DeleteFrame(dynamic_cast<wxsFrameRes*>(this));
            break;
            
        case Panel:
            GetProject()->DeletePanel(dynamic_cast<wxsPanelRes*>(this));
            break;
            
        default:;
    }
}

wxsEditor* wxsWindowRes::CreateEditor()
{
    wxsWindowEditor* Edit = new wxsWindowEditor(Manager::Get()->GetEditorManager()->GetNotebook(),XrcFile,this);
    Edit->BuildPreview(RootWidget);
    return Edit;
}

void wxsWindowRes::Save()
{
    TiXmlDocument* Doc = GenerateXml();
    
    if ( Doc )
    {
        wxString FullFileName = GetProject()->GetInternalFileName(XrcFile);
        Doc->SaveFile(FullFileName.mb_str());
        delete Doc;
    }
}

TiXmlDocument* wxsWindowRes::GenerateXml()
{
    TiXmlDocument* NewDoc = new TiXmlDocument;
    TiXmlElement* Resource = NewDoc->InsertEndChild(TiXmlElement("resource"))->ToElement();
    TiXmlElement* XmlDialog = Resource->InsertEndChild(TiXmlElement("object"))->ToElement();
    XmlDialog->SetAttribute("class",wxString(GetWidgetClass()).mb_str());
    XmlDialog->SetAttribute("name",ClassName.mb_str());
    if ( !RootWidget->XmlSave(XmlDialog) )
    {
        delete NewDoc;
        return NULL;
    }
    return NewDoc;
}

void wxsWindowRes::ShowPreview()
{
    Save();
    
    wxXmlResource Res(GetProject()->GetInternalFileName(XrcFile));
    Res.InitAllHandlers();
    
    switch ( Type )
    {
    	case Dialog:
    	{
            wxDialog Dlg;
            
            if ( Res.LoadDialog(&Dlg,NULL,ClassName) )
            {
                Dlg.ShowModal();
            }
            break;
    	}
            
        case Frame:
        {
            wxFrame* Frm = new wxFrame;
            if ( Res.LoadFrame(Frm,NULL,ClassName) )
            {
            	Frm->Show();
            }
            break;
        }
            
        case Panel:
        {
        	wxDialog Dlg(NULL,-1,wxString::Format(_("Frame preview: %s"),ClassName.c_str()));
        	wxPanel* Panel = Res.LoadPanel(&Dlg,ClassName);
        	if ( Panel )
        	{
        		Dlg.Fit();
        		Dlg.ShowModal();
        	}
        	break;
        }
        
        default:;
    }
}

const wxString& wxsWindowRes::GetResourceName()
{
    return GetClassName();
}

bool wxsWindowRes::GenerateEmptySources()
{
    // Generating file variables
    
    wxString FName = wxFileName(HFile).GetFullName();
    FName.MakeUpper();
    wxString Guard(_T("__"));
    
    for ( int i=0; i<(int)FName.Length(); i++ )
    {
        char ch = FName.GetChar(i);
        if ( ( ch < 'A' || ch > 'Z' ) && ( ch < '0' || ch > '9' ) ) Guard.Append('_');
        else Guard.Append(ch);
    }
    
    wxFileName IncludeFN(GetProject()->GetProjectFileName(HFile));
    IncludeFN.MakeRelativeTo(
        wxFileName(GetProject()->GetProjectFileName(SrcFile)).GetPath() );
    wxString Include = IncludeFN.GetFullPath();
    

    FILE* Fl = fopen(GetProject()->GetProjectFileName(HFile).mb_str(),"wt");
    if ( !Fl ) return false;
    wxString Content = EmptyHeader;
    Content.Replace(_T("$(Guard)"),Guard,true);
    Content.Replace(_T("$(ClassName)"),ClassName,true);
    Content.Replace(_T("$(BaseClassName)"),GetWidgetClass(),true);
    fprintf(Fl,"%s",(const char*)Content.mb_str());
    fclose(Fl);
    
    Fl = fopen(GetProject()->GetProjectFileName(SrcFile).mb_str(),"wt");
    if ( !Fl ) return false;
    Content = EmptySource;
    Content.Replace(_T("$(Include)"),Include,true);
    Content.Replace(_T("$(ClassName)"),ClassName,true);
    Content.Replace(_T("$(BaseClassName)"),GetWidgetClass(),true);
    switch ( Type )
    {
    	case Dialog:
            Content.Replace(_T("$(BaseClassCtor)"),_T("wxDialog(parent,id,wxT(\"\"),wxDefaultPosition,wxDefaultSize)"),true);
            break;
            
        case Frame:
            Content.Replace(_T("$(BaseClassCtor)"),_T("wxFrame(parent,id,wxT(\"\"))"),true);
            break;
            
        case Panel:
            Content.Replace(_T("$(BaseClassCtor)"),_T("wxPanel(parent,id)"),true);
            break;
            
        default:;
    }
    fprintf(Fl,"%s",(const char*)Content.mb_str());
    fclose(Fl);
    return true;
}

void wxsWindowRes::NotifyChange()
{
	assert ( GetProject() != NULL );
	
	UpdateWidgetsVarNameId();
	
	#if 1
	
	int TabSize = 4;
	int GlobalTabSize = 2 * TabSize;
	
	// Generating producing code
	wxsCodeGen Gen(RootWidget,TabSize,TabSize);
	
	// Creating code header

	wxString CodeHeader = wxString::Format(_T("//(*Initialize(%s)"),GetClassName().c_str());
	wxString Code = CodeHeader + _T("\n");
	
	// Creating local and global declarations
	
	wxString GlobalCode;
	bool WasDeclaration = false;
	AddDeclarationsReq(RootWidget,Code,GlobalCode,TabSize,GlobalTabSize,WasDeclaration);
	if ( WasDeclaration )
	{
		Code.Append('\n');
	}
		
	// Creating window-generating code
	
	Code.Append(Gen.GetCode());
	Code.Append(' ',TabSize);
	
// TODO (SpOoN#1#): Apply title and centered properties for frame and dialog
	

	wxsCoder::Get()->AddCode(GetProject()->GetProjectFileName(SrcFile),CodeHeader,Code);
	
	// Creating global declarations
	
	CodeHeader.Printf(_T("//(*Declarations(%s)"),GetClassName().c_str());
	Code = CodeHeader + _T("\n") + GlobalCode;
	Code.Append(' ',GlobalTabSize);
	wxsCoder::Get()->AddCode(GetProject()->GetProjectFileName(HFile),CodeHeader,Code);
	
	// Creating set of ids
	wxArrayString IdsArray;
	BuildIdsArray(RootWidget,IdsArray);
	CodeHeader.Printf(_T("//(*Identifiers(%s)"),GetClassName().c_str());
	Code = CodeHeader;
	Code.Append(_T('\n'));
	Code.Append(_T(' '),GlobalTabSize);
	Code.Append(_T("enum Identifiers\n"));
	Code.Append(_T(' '),GlobalTabSize);
	Code.Append(_T('{'));
	IdsArray.Sort();
	wxString Previous = _T("");
	bool First = true;
	for ( size_t i = 0; i<IdsArray.Count(); ++i )
	{
		if ( IdsArray[i] != Previous )
		{
			Previous = IdsArray[i];
			Code.Append( _T('\n') );
			Code.Append( _T(' '), GlobalTabSize + TabSize );
			Code.Append( Previous );
			if ( First )
			{
				Code.Append( _T(" = 0x1000") );
			}
			Code.Append( _T(',') );
		}
	}
	Code.Append( _T('\n') );
	Code.Append( _T(' '), GlobalTabSize );
	Code.Append( _T("};\n") );
	Code.Append( _T(' '), GlobalTabSize );
	wxsCoder::Get()->AddCode(GetProject()->GetProjectFileName(HFile),CodeHeader,Code);
	
	#endif
}

void wxsWindowRes::AddDeclarationsReq(wxsWidget* Widget,wxString& LocalCode,wxString& GlobalCode,int LocalTabSize,int GlobalTabSize,bool& WasLocal)
{
	static wxsCodeParams EmptyParams;
	
	if ( !Widget ) return;
	int Count = Widget->GetChildCount();
	for ( int i=0; i<Count; i++ )
	{
		wxsWidget* Child = Widget->GetChild(i);
		bool Member = Child->GetBaseParams().IsMember;
		wxString& Code = Member ? GlobalCode : LocalCode;
		
		Code.Append(' ',Member ? GlobalTabSize : LocalTabSize);
		Code.Append(Child->GetDeclarationCode(EmptyParams));
		Code.Append('\n');
		
		WasLocal |= !Member;
		AddDeclarationsReq(Child,LocalCode,GlobalCode,LocalTabSize,GlobalTabSize,WasLocal);
	}
}

inline const wxChar* wxsWindowRes::GetWidgetClass(bool UseRes)
{
	switch ( Type )
	{
		case Dialog: return _T("wxDialog");
		case Frame:  return _T("wxFrame");
		case Panel:  return UseRes ? _T("wxPanelr") : _T("wxPanel");
	}
	
	return _T("");
}

void wxsWindowRes::UpdateWidgetsVarNameId()
{
    StrMap NamesMap;
    StrMap IdsMap;
    
    CreateSetsReq(NamesMap,IdsMap,RootWidget);
   	UpdateWidgetsVarNameIdReq(NamesMap,IdsMap,RootWidget);
}

void wxsWindowRes::UpdateWidgetsVarNameIdReq(StrMap& NamesMap, StrMap& IdsMap, wxsWidget* Widget)
{
	int Cnt = Widget->GetChildCount();
	for ( int i=0; i<Cnt; i++ )
	{
		wxsWidget* Child = Widget->GetChild(i);
		
        wxsWidgetBaseParams& Params = Child->GetBaseParams();
        
        if ( Params.VarName.Length() == 0 || Params.IdName.Length() == 0 )
        {
            wxString NameBase = Child->GetInfo().DefaultVarName;
            wxString Name;
            wxString IdBase = Child->GetInfo().DefaultVarName;
            IdBase.MakeUpper();
            wxString Id;
            int Index = 1;
            do
            {
                Name.Printf(_T("%s%d"),NameBase.c_str(),Index);
                Id.Printf(_T("ID_%s%d"),IdBase.c_str(),Index++);
            }
            while ( NamesMap.find(Name) != NamesMap.end() ||
                    IdsMap.find(Id)     != IdsMap.end() );
            
            Params.VarName = Name;
            NamesMap[Name] = Child;
            Params.IdName = Id;
            IdsMap[Id] = Child;
        }
    
		UpdateWidgetsVarNameIdReq(NamesMap,IdsMap,Child);
	}
}

void wxsWindowRes::CreateSetsReq(StrMap& NamesMap, StrMap& IdsMap, wxsWidget* Widget, wxsWidget* Without)
{
	int Cnt = Widget->GetChildCount();
	for ( int i=0; i<Cnt; i++ )
	{
		wxsWidget* Child = Widget->GetChild(i);
		
		if ( Child != Without )
		{
            if ( Child->GetBaseParams().VarName.Length() )
            {
                NamesMap[Child->GetBaseParams().VarName.c_str()] = Child;
            }
            
            if ( Child->GetBaseParams().VarName.Length() )
            {
                IdsMap[Child->GetBaseParams().VarName.c_str()] = Child;
            }
		}
		
		CreateSetsReq(NamesMap,IdsMap,Child,Without);
	}
}

bool wxsWindowRes::CheckBaseProperties(bool Correct,wxsWidget* Changed)
{
    StrMap NamesMap;
    StrMap IdsMap;
    
    if ( Changed == NULL )
    {
    	// Will check all widgets
    	return CheckBasePropertiesReq(RootWidget,Correct,NamesMap,IdsMap);
    }
    
    // Creating sets of names and ids
   	CreateSetsReq(NamesMap,IdsMap,RootWidget,Changed);
   	
   	// Checkign and correcting changed widget
   	return CorrectOneWidget(NamesMap,IdsMap,Changed,Correct);
}

bool wxsWindowRes::CheckBasePropertiesReq(wxsWidget* Widget,bool Correct,StrMap& NamesMap,StrMap& IdsMap)
{
	bool Result = true;
	int Cnt = Widget->GetChildCount();
	for ( int i=0; i<Cnt; ++i )
	{
		wxsWidget* Child = Widget->GetChild(i);
		
		if ( !CorrectOneWidget(NamesMap,IdsMap,Child,Correct) )
		{
			if ( !Correct ) return false;
			Result = false;
		}
		
		NamesMap[Child->GetBaseParams().VarName] = Child;
		IdsMap[Child->GetBaseParams().IdName] = Child;
		
		if ( ! CheckBasePropertiesReq(Child,Correct,NamesMap,IdsMap) )
		{
			if ( !Correct ) return false;
			Result = false;
		}
	}
	
	return Result;
}

bool wxsWindowRes::CorrectOneWidget(StrMap& NamesMap,StrMap& IdsMap,wxsWidget* Changed,bool Correct)
{
	bool Valid = true;

    // Validating variable name
	
    if ( Changed->GetBPType() & wxsWidget::bptVariable )
    {
    	wxString& VarName = Changed->GetBaseParams().VarName;
    	wxString Corrected;
    	VarName.Trim(true);
    	VarName.Trim(false);
    	
    	// first validating produced name
    	
    	if ( VarName.Length() == 0 )
    	{
    		if ( !Correct )
    		{
    			wxMessageBox(_("Item must have variable name"));
    			return false;
    		}

   			// Creating new unique name
    			
   			const wxString& Prefix = Changed->GetInfo().DefaultVarName;
   			for ( int i=1;; ++i )
   			{
   				Corrected.Printf(_T("%s%d"),Prefix.c_str(),i);
   				if ( NamesMap.find(Corrected) == NamesMap.end() ) break;
   			}

    		Valid = false;
    	}
    	else
    	{
    		// Validating name as C++ ideentifier
            if ( wxString(_T("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                          _T("abcdefghijklmnopqrstuvwxyz")
                          _T("_") ).Find(VarName.GetChar(0)) == -1 )
            {
            	if ( !Correct )
            	{
            		wxMessageBox(wxString::Format(_("Invalid character: '%c' in variable name"),VarName.GetChar(0)));
            		return false;
            	}
                Valid = false;
            }
            else
            {
            	Corrected.Append(VarName.GetChar(0));
            }
            
            for ( size_t i=1; i<VarName.Length(); ++i )
            {
                if ( wxString(_T("0123456789")
                              _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                              _T("abcdefghijklmnopqrstuvwxyz")
                              _T("_") ).Find(VarName.GetChar(i)) == -1 )
                {
                    if ( !Correct )
                    {
                        wxMessageBox(wxString::Format(_("Invalid character: '%c' in variable name"),VarName.GetChar(i)));
                        return false;
                    }
                    Valid = false;
                }
                else
                {
                    Corrected.Append(VarName.GetChar(i));
                }
            }

            // Searching for another widget with same name
            if ( NamesMap.find(Corrected) != NamesMap.end() )
            {
            	if ( !Correct )
            	{
            		wxMessageBox(wxString::Format(_("Item with variable name '%s' already exists"),Corrected.c_str()));
            		return false;
            	}
            	
            	// Generating new unique name

                const wxString& Prefix = Changed->GetInfo().DefaultVarName;
                for ( int i=1;; ++i )
                {
                    Corrected.Printf(_T("%s%d"),Prefix.c_str(),i);
                    if ( NamesMap.find(Corrected) == NamesMap.end() ) break;
                }

            	Valid = false;
            }
    	}
    
        if ( Correct )
        {
        	VarName = Corrected;
        }
    }
    
    if ( Changed->GetBPType() & wxsWidget::bptId )
    {
    	wxString& IdName = Changed->GetBaseParams().IdName;
    	wxString Corrected;
    	IdName.Trim(true);
    	IdName.Trim(false);
    	
    	// first validating produced name
    	
    	if ( IdName.Length() == 0 )
    	{
    		if ( !Correct )
    		{
    			wxMessageBox(_("Item must have identifier"));
    			return false;
    		}

   			// Creating new unique name
    			
   			wxString Prefix = Changed->GetInfo().DefaultVarName;
   			Prefix.UpperCase();
   			for ( int i=1;; ++i )
   			{
   				Corrected.Printf(_T("ID_%s%d"),Prefix.c_str(),i);
   				if ( IdsMap.find(Corrected) == IdsMap.end() ) break;
   			}

    		Valid = false;
    	}
    	else
    	{
    		// Validating name as C++ ideentifier
            if ( wxString(_T("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                          _T("abcdefghijklmnopqrstuvwxyz")
                          _T("_") ).Find(IdName.GetChar(0)) == -1 )
            {
            	if ( !Correct )
            	{
            		wxMessageBox(wxString::Format(_("Invalid character: '%c' in variable name"),IdName.GetChar(0)));
            		return false;
            	}
                Valid = false;
            }
            else
            {
            	Corrected.Append(IdName.GetChar(0));
            }
            
            for ( size_t i=1; i<IdName.Length(); ++i )
            {
                if ( wxString(_T("0123456789")
                              _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                              _T("abcdefghijklmnopqrstuvwxyz")
                              _T("_") ).Find(IdName.GetChar(i)) == -1 )
                {
                    if ( !Correct )
                    {
                        wxMessageBox(wxString::Format(_("Invalid character: '%c' in variable name"),IdName.GetChar(i)));
                        return false;
                    }
                    Valid = false;
                }
                else
                {
                    Corrected.Append(IdName.GetChar(i));
                }
            }

            // Searching for another widget with same name
            
            if ( IdsMap.find(Corrected) != IdsMap.end() && Corrected != _T("ID_COMMON") )
            {
            	if ( !Correct )
            	{
            		wxMessageBox(wxString::Format(_("Item with identifier '%s' already exists"),Corrected.c_str()));
            		return false;
            	}
            	
            	// Generating new unique name

                wxString Prefix = Changed->GetInfo().DefaultVarName;
                Prefix.UpperCase();
                for ( int i=1;; ++i )
                {
                    Corrected.Printf(_T("ID_%s%d"),Prefix.c_str(),i);
                    if ( IdsMap.find(Corrected) == IdsMap.end() ) break;
                }

            	Valid = false;
            }
    	}
    	
    	if ( Correct )
    	{
    		IdName = Corrected;
    	}
    }
    
	return Valid;
}

void wxsWindowRes::BuildIdsArray(wxsWidget* Widget,wxArrayString& Array)
{
	int Cnt = Widget->GetChildCount();
	for ( int i=0; i<Cnt; i++ )
	{
		wxsWidget* Child = Widget->GetChild(i);
		if ( Child->GetBPType() & wxsWidget::bptId )
		{
			Array.Add(Child->GetBaseParams().IdName);
		}
		BuildIdsArray(Child,Array);
	}
}
