#include "CardReader.h"
#include <cstdio>
#include <cstring>

using namespace std;

vector<string> CardReader::list() {
	vector<string> out;
	SCARDCONTEXT ctx;

	if (SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &ctx) != SCARD_S_SUCCESS) {
		return out;
	}

	DWORD len = 0;
	if (SCardListReadersA(ctx, nullptr, nullptr, &len) == SCARD_S_SUCCESS) {
		vector<char> buf(len);
		SCardListReadersA(ctx, nullptr, buf.data(), &len);
		for (const char* p = buf.data(); *p; p += strlen(p) + 1) {
			out.emplace_back(p);
		}
	}
	SCardReleaseContext(ctx);
	return out;
}

bool CardReader::connect(const string& name) {
	disconnect();
	if (SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &ctx_) != SCARD_S_SUCCESS)
		return false;

	if (SCardConnectA(ctx_, name.c_str(), SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &card_, &proto_) != SCARD_S_SUCCESS) { 
		log("CONNECT FAIL: " + name);
		disconnect();
		return false;
	}
	log("CONNECT OK: " + name);
	return true;
}

void CardReader::attach(SCARDHANDLE h) {
	disconnect();
	owned_ = false;
	card_ = h;
	proto_ = SCARD_PROTOCOL_T1;   // 取不到實際協定時的預設值

	// 向 PC/SC 查詢這條連線目前實際使用的協定(T=0 / T=1)
	char name[256];
	BYTE atr[36];
	DWORD nameLen = sizeof name, state = 0, proto = 0, atrLen = sizeof atr;
	if (SCardStatusA(card_, name, &nameLen, &state, &proto, atr, &atrLen) == SCARD_S_SUCCESS &&
		(proto == SCARD_PROTOCOL_T0 || proto == SCARD_PROTOCOL_T1))
		proto_ = proto;
}

void CardReader::disconnect() {
	if (card_ && owned_) {
		SCardDisconnect(card_, SCARD_LEAVE_CARD);
		log("DISCONNECT");
	}
	if (ctx_)  SCardReleaseContext(ctx_);
	card_ = 0;
	ctx_ = 0;
	owned_ = true;
}

bool CardReader::transmit(const Bytes& cmd, Bytes& data, uint16_t& sw) {
	if (!card_) return false;

	log("TX  " + toHex(cmd));

	vector<BYTE> buf(65538);   // Extended APDU 回應最大 64KB + SW
	DWORD len = (DWORD)buf.size();
	LONG rc = SCardTransmit(card_,
		proto_ == SCARD_PROTOCOL_T0 ? SCARD_PCI_T0 : SCARD_PCI_T1,
		cmd.data(), (DWORD)cmd.size(), nullptr, buf.data(), &len);

	if (rc != SCARD_S_SUCCESS || len < 2) {
		char b[64];
		snprintf(b, sizeof b, "ERR SCardTransmit rc=0x%08lX", (unsigned long)rc);
		log(b);
		return false;
	}

	log("RX  " + toHex(Bytes(buf.begin(), buf.begin() + len)));
	sw = (uint16_t)((buf[len - 2] << 8) | buf[len - 1]);
	data.assign(buf.begin(), buf.begin() + len - 2);

	// T=0 常見:SW1=61 代表還有資料要取,自動送 GET RESPONSE
	if ((sw >> 8) == 0x61) {
		return transmit(Bytes{ cmd[0], 0xC0, 0x00, 0x00, (uint8_t)(sw & 0xFF) }, data, sw);
	}
	return true;
}