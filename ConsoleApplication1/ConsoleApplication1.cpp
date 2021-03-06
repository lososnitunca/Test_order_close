#include "stdafx.h"

std::mutex mute;

class CManager
{
private:
	CManagerFactory   m_factory;
	CManagerInterface *m_manager;
public:
	CManager() : m_factory("mtmanapi.dll"), m_manager(NULL)
	{
		m_factory.WinsockStartup();
		if (m_factory.IsValid() == FALSE || (m_manager = m_factory.Create(ManAPIVersion)) == NULL)
		{
			std::cout << "Failed to create MetaTrader 4 Manager API interface" << std::endl;
			return;
		}
	}
	~CManager()
	{
		if (m_manager != NULL)
		{
			if (m_manager->IsConnected())
				m_manager->Disconnect();
			m_manager->Release();
			m_manager = NULL;
		}
		m_factory.WinsockCleanup();
	}
	bool IsValid()
	{
		return(m_manager != NULL);
	}
	CManagerInterface* operator->()
	{
		return(m_manager);
	}
};


class ParserInit // init class for parsing ini file
{
private:
	boost::property_tree::ptree iniTree;
	std::string m_server;
	int m_login;
	std::string m_password;
	int m_time;
	std::vector<int> m_Logins{ 0 };
	std::string m_path;

public:
	ParserInit(std::string path = "conf.ini") : m_path(path) // we believe that we have Server, Manager, Timer, Logins sections in file
	{
		try
		{
			boost::property_tree::read_ini(m_path, iniTree);

			boost::property_tree::ptree& iniServer = iniTree.get_child("Server");
			boost::property_tree::ptree& iniManager = iniTree.get_child("Manager");
			boost::property_tree::ptree& iniTimer = iniTree.get_child("Timer");
			boost::property_tree::ptree& iniLogins = iniTree.get_child("Logins");

			m_server = iniServer.get<std::string>("adress");
			m_login = iniManager.get<int>("login");
			m_password = iniManager.get<std::string>("password");
			m_time = iniTimer.get<int>("time");

			int counter = iniLogins.size(); // init list of logins opening orders
			m_Logins.resize(counter);
			counter = 0;
			for (auto&i : iniLogins)
			{
				m_Logins[counter] = i.second.get<int>("");
				++counter;
			}
			std::sort(m_Logins.begin(), m_Logins.end());
		}
		catch (boost::property_tree::ini_parser_error)
		{
			std::cout << "No ini file or bad file structure" << std::endl << std::endl;
		}
	}
	LPCSTR adress() // ip:port function
	{
		LPCSTR server = m_server.c_str();
		return server;
	}
	int login() // manager login function
	{
		return m_login;
	}
	LPCSTR password() // manager password function
	{
		LPCSTR password = m_password.c_str();
		return password;
	}
	int timer() // closing timer function
	{
		return m_time;
	}
	std::vector<int> logins() // list of logins opening orders function
	{
		return m_Logins;
	}
};

class MTfunctions // class for project functions
{
private:
	static void OrderCloseThread(void *param, int order) // thread for closing orders
	{
		m_param *tlparam = (m_param*)param;
		int *ptimer = &tlparam->ms_time;

		CManager *pump_man = &tlparam->pump_manager; //this init construct for pointer to CManager objects
		CManager &pump_manager = *pump_man;

		CManager *man = &tlparam->manager;
		CManager &manager = *man;

		int res = RET_ERROR;

		TradeRecord trade = { 0 };
		TradeTransInfo trans = { 0 };
		TickInfo *info = NULL;

		int total = 0;
		int f_sleep = 0;

		trans.type = TT_BR_ORDER_CLOSE; //trade transaction type - manager request to close order
		trans.order = order;

		Sleep((*ptimer) * 1000); // this point needs for time interval before closing and because all time is in milisec so we must multiplicate our time counter in 1000
		if ((res = pump_manager->TradeRecordGet(trans.order, &trade)) != RET_OK)
		{
			std::cout << "Can`t take volume " << res << manager->ErrorDescription(res) << std::endl;
		}
		else
		{
			info = pump_manager->TickInfoLast(trade.symbol, &total);
			if (total == 0)
			{
				std::cout << "Close price taken failed: " << trade.symbol << std::endl;
			}
			else
			{
				switch (trade.cmd)
				{
				case OP_BUY:
					trans.price = info->bid;
					break;
				case OP_SELL:
					trans.price = info->ask;
					break;
				}
				trans.volume = trade.volume;
				std::cout << "Close parametres: order: " << trans.order << " price: " << trans.price << " volume: " << trans.volume << " stime: " << manager->ServerTime() << std::endl;
				if ((res = manager->TradeTransaction(&trans)) != RET_OK)
				{
					std::cout << "Order closing failed: " << res << manager->ErrorDescription(res) << std::endl;
				}
				else std::cout << "Order " << trans.order << " was closed by programm" << std::endl;
			}
		}

		while (tlparam->s_buffer[0].s_order)
		{
			trans = { 0 };
			trade = { 0 };
			trans.order = tlparam->s_buffer[0].s_order;
			trans.type = TT_BR_ORDER_CLOSE;

			if ((f_sleep = tlparam->s_buffer[0].s_close_time - manager->ServerTime()) > 0)
			{
				std::cout << tlparam->s_buffer[0].s_close_time << " UND " << manager->ServerTime() << " Next sleep time: " << f_sleep << std::endl;
				Sleep(f_sleep * 1000);
			}

			if ((res = pump_manager->TradeRecordGet(trans.order, &trade)) != RET_OK)
			{
				std::cout << "Can`t take volume " << res << manager->ErrorDescription(res) << std::endl;
			}
			else
			{
				info = pump_manager->TickInfoLast(trade.symbol, &total);
				if (total == 0)
				{
					std::cout << "Close price taken failed: " << trade.symbol << std::endl;
				}
				else
				{
					switch (trade.cmd)
					{
					case OP_BUY:
						trans.price = info->bid;
						break;
					case OP_SELL:
						trans.price = info->ask;
						break;
					}
					trans.volume = trade.volume;
					std::cout << "Close parametres: order: " << trans.order << " price: " << trans.price << " volume: " << trans.volume << std::endl;
					if ((res = manager->TradeTransaction(&trans)) != RET_OK)std::cout << "Order closing failed: " << res << manager->ErrorDescription(res) << std::endl;
					else std::cout << "Order " << trans.order << " was closed by programm" << std::endl;
				}
			}
			std::lock_guard<std::mutex> lock(mute); // not sure but protect buffer and counter from changes at the same time with changing it in pumping
			{
				for (int i = 0; i < tlparam->counter; ++i)tlparam->s_buffer[i] = tlparam->s_buffer[i + 1];
				if (tlparam->counter > 0)--tlparam->counter;
				else tlparam->counter = 0;
			}
		}
		tlparam->life = FALSE;
		pump_manager->MemFree(info);
	}
	static void OrderClose(void *param, void *data) // function for creation thread for chosen orders
	{
		TradeRecord *trades = (TradeRecord*)data;

		int order = trades->order;

		std::thread close(OrderCloseThread, param, order);
		close.detach();
	}
public:
	struct buffer // trades buffer struct
	{
		int s_order;
		int s_open_time;
		int s_close_time;
	};
	struct m_param // struct for orders functions = parametr "param"
	{
		int ms_time; // timer form ini file
		std::vector<int> ms_Logins; // list of traders logins whose logins are tracked
		CManager manager;
		CManager pump_manager;
		std::array<buffer, 1000> s_buffer; // trades buffer
		bool life; //counter for buffer: empty or not
		int counter; // counter for buffer elements
	};
	static void ManagerPingThread(void *manager) // ping function for active connection to MT4Server
	{
		CManager *p_manager = (CManager*)manager;
		CManager &r_manager = *p_manager;
		while (TRUE)
		{
			r_manager->Ping();
			std::cout << "<<<<<<<<<<PINGGGUUUUUUUUUUUUUUUUUUUUU!!!!>>>>>>>>>>" << std::endl;
			Sleep(120000); //ping every 2 minutes
		}
	}
	static void _stdcall PumpingNotify(int code, int type, void *data, void *param) // pumping callback function, in parametr param we have struct with time and logins list
	{
		if (code == PUMP_UPDATE_TRADES && data != NULL) // check only trades update
		{
			TradeRecord *trades = (TradeRecord*)data; // init trades array
			m_param *tlparam = (m_param*)param; // init param struct
			int *t_param = &tlparam->ms_time;
			std::vector<int> logins = tlparam->ms_Logins; // exclude logins from param struct
			switch (type)
			{
			case TRANS_ADD: // massage order add and check condition of closing

				std::cout << "Created " << trades->order << " order by user: " << trades->login << std::endl;
				if (std::binary_search(logins.begin(), logins.end(), trades->login))
				{
					if (tlparam->life)
					{
						tlparam->s_buffer[tlparam->counter].s_order = trades->order;
						tlparam->s_buffer[tlparam->counter].s_open_time = trades->open_time;
						tlparam->s_buffer[tlparam->counter].s_close_time = trades->open_time + (*t_param);
						++tlparam->counter;
					}
					else
					{
						tlparam->life = TRUE;
						OrderClose(param, data);
					}
				}
				else std::cout << "It was other user = " << trades->login << std::endl;
				break;
			case TRANS_DELETE: // message order deleted
				std::cout << "Order " << trades->order << " was closed!" << std::endl;
				break;
			}
		}
	}
};

int main()
{
	const std::string fconfig = "conf.ini";
	ParserInit Init(fconfig); // initialization ini file configurations
	if (Init.login() == 0)
	{
		system("pause");
		return 10;
	}

	std::cout << "MetaTrader 4 Manager API: Test project" << std::endl;
	std::cout << "Close opened orders by timer for users" << std::endl;
	std::cout << "Copyright ebanye sobaki s osteohondrozom" << std::endl << std::endl;

	int res = RET_ERROR;

	MTfunctions::m_param param; // CManager objects init: "man" for closing orders and "pump_man" for pumping and timer and logins gets to pumping as param
	param.ms_time = Init.timer();
	param.ms_Logins = Init.logins();
	param.life = FALSE;
	param.s_buffer = { NULL };
	param.counter = 0;

	if (((res = param.manager->Connect(Init.adress())) != RET_OK) || ((res = param.manager->Login(Init.login(), Init.password())) != RET_OK))
	{
		std::cout << "Connect to " << Init.adress() << " as " << Init.login() << " failed " << param.manager->ErrorDescription(res) << std::endl;
		system("pause");
		return RET_ERROR;
	}
	else
	{
		if (((res = param.pump_manager->Connect(Init.adress())) != RET_OK) || ((res = param.pump_manager->Login(Init.login(), Init.password())) != RET_OK))
		{
			std::cout << "Connect to " << Init.adress() << " as " << Init.login() << " failed " << param.pump_manager->ErrorDescription(res) << std::endl;
			param.manager->Disconnect();
			system("pause");
			return RET_ERROR;
		}
	}

	std::cout << "Connect to " << Init.adress() << " as " << Init.login() << " successfully" << std::endl;

	std::thread manager_ping(MTfunctions::ManagerPingThread, &(param.manager)); // create ping thread
	manager_ping.detach();

	MTfunctions::m_param* p_param = &param; // make link to give param struct as pumpimg parameter

	if ((res = param.pump_manager->PumpingSwitchEx(MTfunctions::PumpingNotify, 0, p_param)) != RET_OK)
	{
		std::cout << "Pumping failed" << std::endl;
		system("pause");
		param.manager->Disconnect();
		param.pump_manager->Disconnect();
		return RET_ERROR;
	}
	else std::cout << "Pumping switched successfully" << std::endl;

	system("pause");

	param.manager->Disconnect();
	param.pump_manager->Disconnect();

	return 0;
}