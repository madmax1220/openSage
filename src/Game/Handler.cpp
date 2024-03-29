// Stephan Vedder 2015
#include "Handler.hpp"
#include "GameData.hpp"
#include "INI.hpp"
#include "../Loaders/BigStream.hpp"
#include "../FileSystem.hpp"
#include <memory>

using namespace Loaders;
using namespace Game;

//TODO: change this for different gametitles
//currently set for bfme2
std::vector<Handler::LoadInfo> Handler::loadOrder = {
	Handler::LoadInfo("EALogoMovie", new Handler::CinematicArgs(false)),
	Handler::LoadInfo("NewLineLogo", new Handler::CinematicArgs(false)),
	Handler::LoadInfo("TolkienLogo", new Handler::CinematicArgs(false)),
	Handler::LoadInfo("Overall_Game_Intro", new Handler::CinematicArgs(true)),
	Handler::LoadInfo("LoadingRing", new Handler::LoadingScreenArgs("titlescreenuserinterface.jpg")),
	Handler::LoadInfo("MainMenu", new Handler::AptArgs(""))
};

std::shared_ptr<Handler::StateInfo> Handler::cState;
int Handler::state_index = 0;
bool Handler::escPressed = false;

Handler::CinematicInfo::CinematicInfo(std::shared_ptr<Loaders::Vp6Stream> video, std::shared_ptr<Loaders::Mp3Stream> audio,bool skipable)
{
	vp6 = video;
	mp3 = audio;
	canSkip = skipable;
	state_type = CINEMATIC;
}

Handler::LoadingScreenInfo::LoadingScreenInfo(std::shared_ptr<Loaders::Vp6Stream> video, std::shared_ptr<sf::Texture> texture)
{
	vp6 = video;
	tex = texture;
	start = std::chrono::high_resolution_clock::now();
	state_type = LOADING_SCREEN;
}

Handler::AptInfo::AptInfo(std::shared_ptr<Loaders::AptFile> aptfile)
{
	apt = aptfile;
	state_type = APT_FILE;
}

void Handler::Initialize()
{
	BigStream iniStream;
	std::string iniStr;
	iniStream.open(GameData::videoINI);
	iniStr = iniStream.readAll();
	INI::Parse(iniStr);
	iniStream.open(GameData::speechINI);
	iniStr = iniStream.readAll();
	INI::Parse(iniStr);
	iniStream.open(GameData::languageINI);
	iniStr = iniStream.readAll();
	INI::Parse(iniStr);
	GetState();	
}

void Handler::GetState()
{
	bool done = false;

	if (loadOrder.back().loaded == true)
	{
		std::cout << "Finished game" << std::endl;
	}
	
	for (auto& l : loadOrder)
	{
		if (l.loaded)
			continue;
	
		l.loaded = true;

		switch (l.args->GetType())
		{
		case CINEMATIC:
		{			
			auto args =	dynamic_cast<CinematicArgs*>(l.args);
			auto video = GameData::GetVideo(l.name);
			if (video == nullptr)
				continue;

			std::shared_ptr<Loaders::Vp6Stream> vp6 = std::make_shared<Loaders::Vp6Stream>();
			if (!vp6->open(GameData::videoDIR + video->filename + ".vp6"))
				continue;

			vp6->play();

			std::shared_ptr<Loaders::Mp3Stream> mp3 = std::make_shared<Loaders::Mp3Stream>();
			auto dialog = GameData::GetDialogEvent(video->filename);
			if (dialog != nullptr)
			{							
				std::transform(dialog->filename.begin(), dialog->filename.end(), dialog->filename.begin(), ::tolower);

				if (mp3->open(GameData::speechDIR + dialog->filename))
				{
					mp3->play();
				}
			}
			done = true;

			cState = std::make_shared<CinematicInfo>(vp6, mp3, args->IsSkipable());
			
		}
			break;
		case APT_FILE:
		{
			auto args = dynamic_cast<AptArgs*>(l.args);

			std::shared_ptr<Loaders::AptFile> apt = std::make_shared<Loaders::AptFile>();
			BigStream aptStream;
			if (!aptStream.open(l.name + ".apt"))
				continue;

			BigStream constStream;
			if (!constStream.open(l.name + ".const"))
				continue;

			apt->loadFromStream(aptStream, constStream,l.name);
			done = true;

			cState = std::make_shared<AptInfo>(apt);
		}
			break;
		case LOADING_SCREEN:
		{
			auto args = dynamic_cast<LoadingScreenArgs*>(l.args);
			auto video = GameData::GetVideo(l.name);

			if (video == nullptr)
				continue;

			std::shared_ptr<Loaders::Vp6Stream> vp6 = std::make_shared<Loaders::Vp6Stream>();

			if (vp6->open(GameData::videoDIR + video->filename + ".vp6"))
			{
				vp6->play();
			}

			std::string path = GameData::compiledtexDIR + args->GetImageName().substr(0, 2) + "/" + args->GetImageName();
			BigStream imgFile;
			if (!imgFile.open(path))
				continue;

			std::shared_ptr<sf::Texture> tex = std::make_shared<sf::Texture>();
			tex->loadFromStream(imgFile);

			done = true;

			cState = std::make_shared<LoadingScreenInfo>(vp6, tex);
		}
			break;
		default:
			break;
		}

		if (done)
			break;
	}
}

void Handler::Update(sf::RenderWindow& m_window)
{
	if (!cState->IsDone())
	{
		switch (cState->GetType())
		{
		case CINEMATIC:
		{
			m_window.clear();
			auto cinematic = std::static_pointer_cast<CinematicInfo>(cState);			
			auto sprite = sf::Sprite(cinematic->GetVP6()->GetTexture());
			sprite.setScale((float)m_window.getSize().x / sprite.getTextureRect().width,
							(float)m_window.getSize().y / sprite.getTextureRect().height);
			
			m_window.draw(sprite);
			
			if (escPressed)
			{
				escPressed = false;
				cinematic->Skip();
			}
		}
			break;
		case LOADING_SCREEN:
		{
			m_window.clear();
			auto loading_screen = std::static_pointer_cast<LoadingScreenInfo>(cState);
			auto sprite = sf::Sprite(*loading_screen->GetTex());
			sprite.setScale((float)m_window.getSize().x / sprite.getTextureRect().width,
				(float)m_window.getSize().y / sprite.getTextureRect().height*1.3333f);

			m_window.draw(sprite);
		}
			break;
		case APT_FILE:
		{
			auto apt_file = std::static_pointer_cast<AptInfo>(cState);
			auto apt = apt_file->GetApt();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			apt->Update();
			apt->Render(m_window);		
		}
			break;
		}
	}
	else
	{
		GetState();
	}
}

void Handler::KeyDown(const sf::Event::KeyEvent& keyEv)
{
	switch (keyEv.code)
	{
	case sf::Keyboard::Escape:
		escPressed = true;
		break;
	default:
		break;
	}
}

void Handler::KeyUp(const sf::Event::KeyEvent& keyEv)
{
	switch (keyEv.code)
	{
	case sf::Keyboard::Escape:
		escPressed = false;
		break;
	default:
		break;
	}
}