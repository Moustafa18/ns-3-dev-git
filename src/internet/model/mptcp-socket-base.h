/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Sussex
 * Copyright (c) 2015 Université Pierre et Marie Curie (UPMC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Matthieu Coudron <matthieu.coudron@lip6.fr>
 *          Morteza Kheirkhah <m.kheirkhah@sussex.ac.uk>
 */
#ifndef MPTCP_SOCKET_BASE_H
#define MPTCP_SOCKET_BASE_H

#include "ns3/callback.h"
#include "ns3/mptcp-mapping.h"
#include "ns3/tcp-socket-base.h"
//#include "mp-tcp-subflow.h"

//#include "ns3/mp-tcp-cc.h"
#include "ns3/inet-socket-address.h"
#include "ns3/mptcp-scheduler-round-robin.h"

//using namespace std;

namespace ns3
{
class Ipv4EndPoint;
class Node;
class Packet;
class TcpL4Protocol;
class MpTcpPathIdManager;
class MpTcpSubflow;
//class MpTcpSchedulerRoundRobin;
//class MpTcpCongestionControl;
class TcpOptionMpTcpDSS;
class TcpOptionMpTcpJoin;
class OutputStreamWrapper;


/**
TODO move all the supplementary stuff to MpTcpSocketState
remove the bind, in fact this socket should not care about m_endPoint
so it could be removed as well
*/
//class MpTcpSocketState : TcpSocketState
//{
//
//public:
//    MpTcpSocketState();
//    ~MpTcpSocketState();
//
//};

/**
 * \class MpTcpSocketBase

This is the MPTCP meta socket the application talks with
this socket. New subflows, as well as the first one (the master
socket) are linked to this meta socket.

Every data transfer happens on a subflow.
Following the linux kernel from UCL (http://multipath-tcp.org) convention,
the first established subflow is called the "master" subflow.

This inherits TcpSocketBase so that it can be  used as any other TCP variant:
this is the backward compability feature that is required in RFC.
Also doing so allows to run TCP tests with MPTCP via for instance the command
Config::SetDefault ("ns3::TcpL4Protocol::SocketType", "ns3::MpTcpOlia");

But to make sure some inherited functions are not improperly used, we need to redefine them so that they
launch an assert. You can notice those via the comments "//! Disabled"

As such many inherited (protected) functions are overriden & left empty.


As in linux, the meta should return the m_endPoint information of the master,
even if that subflow got closed during the MpTcpConnection.




ConnectionSucceeded may be called twice; once when it goes to established
and the second time when it sees
 Simulator::ScheduleNow(&MpTcpSocketBase::ConnectionSucceeded, this);

TODO:
-rename in MetaSocket ?
-Ideally it could & should inherit from TcpSocket rather TcpSocketBase
-should use 64 bits based buffer / sq nb

**/
class MpTcpSocketBase :
    public TcpSocketBase
// public TcpSocket

{
public:
  /** public methods
  TODO this could be done differently
  **/
  typedef std::vector< Ptr<MpTcpSubflow> > SubflowList;

  static TypeId GetTypeId(void);

  virtual TypeId GetInstanceTypeId (void) const;

  MpTcpSocketBase();
  MpTcpSocketBase(const MpTcpSocketBase&);
  MpTcpSocketBase(const TcpSocketBase&);

  virtual ~MpTcpSocketBase();

  /**
   * Should be called only by subflows when they update their receiver window
   */
  virtual bool UpdateWindowSize(const TcpHeader& header);

protected:
   ////////////////////////////////////////////
   /// List of overriden callbacks
   ////////////////////////////////////////////
  /**
   * This is callback called by subflow NotifyNewConnectionCreated. If
   * the calling subflow is the master, then the call is forwarded through meta's
   * NotifyNewConnectionCreated, else it is forward to the JoinCreatedCallback
   *
   * \see Socket::NotifyNewConnectionCreated
   */
  virtual void OnSubflowCreated (Ptr<Socket> socket, const Address &from);
  virtual void OnSubflowConnectionFailure (Ptr<Socket> socket);
  virtual void OnSubflowConnectionSuccess (Ptr<Socket> socket);

public:
  /**
  these callbacks will be passed on to
   * \see Socket::Set
   */
  virtual void
  SetSubflowAcceptCallback(Callback<bool, Ptr<MpTcpSocketBase>, const Address &, const Address & > connectionRequest,
                           Callback<void, Ptr<MpTcpSubflow> > connectionCreated
                           );

  virtual void
  SetSubflowConnectCallback(Callback<void, Ptr<MpTcpSubflow> > connectionSucceeded,
                           Callback<void, Ptr<MpTcpSubflow> > connectionFailure
                            );

//  virtual void
//  SetJoinCreatedCallback(Callback<void, Ptr<MpTcpSubflow> >);

  /**
   * Triggers callback registered by SetSubflowAcceptCallback
   */
  void
  NotifySubflowCreated(Ptr<MpTcpSubflow> sf);

  /**
   * Triggers callback registered by SetSubflowConnectCallback
   */
  void
  NotifySubflowConnected(Ptr<MpTcpSubflow> sf);

  /**
   * Called when a subflow TCP state is updated.
   * It detects such events by tracing its subflow m_state.
   *
   */
  virtual void
  OnSubflowNewCwnd(std::string context, uint32_t oldCwnd, uint32_t newCwnd);

  /**
   * Initiates a new subflow with MP_JOIN
   *
   * Wrapper that just creates a subflow, bind it to a specific address
   * and then establishes the connection
   */
  virtual int
  ConnectNewSubflow(const Address &local, const Address &remote);

  virtual void
  DoRetransmit();

  /**
   * Called when a subflow TCP state is updated.
   * It detects such events by tracing its subflow m_state.
   *
   * \param context
   * \param sf Subflow that changed state
   * \param oldState previous TCP state of the subflow
   * \param newState new TCP state of the subflow
   */
  virtual void
  OnSubflowNewState(
    std::string context,
    Ptr<MpTcpSubflow> sf,
    TcpStates_t  oldState,
    TcpStates_t newState
    );

  // Window Management
  virtual uint32_t
  BytesInFlight();  // Return total bytes in flight of a subflow

  void
  DumpRxBuffers(Ptr<MpTcpSubflow> sf) const;

//  void
//  ProcessWait(Ptr<Packet> packet, const TcpHeader& tcpHeader);

//  static void
//  GenerateTokenForKey( mptcp_crypto_t alg, uint64_t key, uint32_t& token, uint64_t& idsn);

  /* Sum congestio nwindows across subflows to compute global cwin
  WARNING: it does not take flows that are closing yet so that may be a weakness depending on the scenario
  to update
  TODO: should be done in the congestion controller
  */
  virtual uint32_t
  ComputeTotalCWND();

  virtual uint16_t
  AdvertisedWindowSize();

  // TODO remove
  virtual uint32_t
  AvailableWindow();
  /**
  \warn This function should be called once a connection is established else
  **/
//  virtual bool IsMpTcpEnabled() const;


  virtual void CloseAndNotify(void);

  /** Limit the size of in-flight data by cwnd and receiver's rxwin */
  virtual uint32_t
  Window (void);

  virtual void
  PersistTimeout();

  /* equivalent to PeerClose
  \param finalDsn
  OnDataFin
  */
  virtual void
  PeerClose( SequenceNumber32 fin_seq, Ptr<MpTcpSubflow> sf);

  virtual void
  OnInfiniteMapping(Ptr<TcpOptionMpTcpDSS> dss, Ptr<MpTcpSubflow> sf);

  /* equivalent to TCP Rst */
  virtual void
  SendFastClose(Ptr<MpTcpSubflow> sf);

  /**
  \brief Generates random key, setups isdn and token
  **/
//  virtual uint64_t GenerateKey();

  virtual void InitLocalISN(const SequenceNumber32& seq);

  /**
  * TODO when is it considered
  * move to TcpSocketBase ?
  * \return
  */
  bool IsConnected() const;

  // Public interface for MPTCP
  // Disabled
  virtual int Bind();
  virtual int Bind(const Address &address);
  virtual int Connect(const Address &address);

  // TODO to remove there is no equivalent in parent's class
//  virtual int Connect(Ipv4Address servAddr, uint16_t servPort);

  //! Disabled
  virtual int Listen(void);

  /**
   * \brief Same as MpTcpSocketBase::Close
   *
   * The default behavior is to do nothing until all the data is transmitted.
   * Only then are
  RFC 6824:
   - When an application calls close() on a socket, this indicates that it
   has no more data to send; for regular TCP, this would result in a FIN
   on the connection.  For MPTCP, an equivalent mechanism is needed, and
   this is referred to as the DATA_FIN.

   - A DATA_FIN has the semantics and behavior as a regular TCP FIN, but
   at the connection level.  Notably, it is only DATA_ACKed once all
   data has been successfully received at the connection level
  */
  // socket function member
  virtual int Close(void);

  /**
  RFC 6824
  - If all subflows have
   been closed with a FIN exchange, but no DATA_FIN has been received
   and acknowledged, the MPTCP connection is treated as closed only
   after a timeout.  This implies that an implementation will have
   TIME_WAIT states at both the subflow and connection levels (see
   Appendix C).  This permits "break-before-make" scenarios where
   connectivity is lost on all subflows before a new one can be re-
   established.
  */
//  virtual void
//  PeerClose(Ptr<Packet>, const TcpHeader&); // Received a FIN from peer, notify rx buffer

  /* called by subflow when it sees a DSS with the DATAFIN flag
  Params are ignored
  \param packet Ignored
  */
  virtual void
  PeerClose(Ptr<Packet> packet, const TcpHeader&); // Received a FIN from peer, notify rx buffer
  virtual void
  DoPeerClose(void); // FIN is in sequence, notify app and respond with a FIN

  virtual int DoClose(void);
//  virtual void
//  CloseAndNotify(void); // To CLOSED state, notify upper layer, and deallocate end point

  virtual uint32_t GetTxAvailable() const;
  virtual uint32_t GetRxAvailable(void) const;

  // TODO maybe this could be removed ?
//  void DoForwardUp(Ptr<Packet> packet, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface);
  virtual void ForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface);

  /**
  \return Number of connected subflows (that is that ran the 3whs)
  */
  SubflowList::size_type GetNActiveSubflows() const;


  /**
   *
   * \return an established subflow
   */
  virtual Ptr<MpTcpSubflow> GetSubflow(uint8_t pos) const;
  
  /**
   * \param addrId Address id as registered in path manager
   * \assert if could not find subflow
   */
  virtual Ptr<MpTcpSubflow> GetSubflowFromAddressId (uint8_t addrId) const;
  
  // can Potentially be removed ?
  virtual void ClosingOnEmpty(TcpHeader& header);

  /**
   * \brief Send a fast close
   *
   * Sends RST on all subflows
   * and MP_FASTCLOSE on one of the subflows
   */
  virtual void SendRST(void);

  /**
   * The connection is considered fully established
   * when it can create new subflows, i.e., when it received
   * a first dss ack
   */
  virtual bool FullyEstablished() const;

  /**
   * This retriggers Connection success callback
   * You have to check in the callback if it fully estalbished or not
   */
  virtual void BecomeFullyEstablished();

  /** TODO superseed into TcpSocketBase
  Here it sends MP_FIN
   */
  virtual void SendFin();

  /**
  public equivalent ?
  * \brief
  * \param srcAddr Address to bind to. In theory Can be an InetSocketAddress or an Inet6SocketAddress
  * for now just InetSocketAddress
  */
  Ptr<MpTcpSubflow> CreateSubflow(bool masterSocket);

  /**
   * Initiate
   * TODO rename into SetupSubflow ?
   */
  virtual void AddSubflow(Ptr<MpTcpSubflow> sf);


  // Path management related functions

//  virtual int GenerateToken(uint32_t& token ) const;

  virtual void Destroy(void);
  /**
  \return 0 In case of success
  TODO bool ?
  **/
  //int GetPeerKey(uint64_t& remoteKey) const;
  uint64_t GetPeerKey() const;

  /**
  \brief Generated during the initial 3 WHS
  */
  uint64_t GetLocalKey() const;

  /**
   * \return Hash of the local key
   */
  uint32_t GetLocalToken() const;

  /**
   * \return Hash of the peer key
   */
  uint32_t GetPeerToken() const;

    /**
  For now it looks there is no way to know that an ip interface went up so we will assume until
  further notice that IPs of the client don't change.
  -1st callback called on receiving an ADD_ADDR
  -2nd callback called on receiving REM_ADDR
  (TODO this class should automatically register)
  **/
  void SetNewAddrCallback(Callback<bool, Ptr<Socket>, Address, uint8_t> remoteAddAddrCb,
                          Callback<void, uint8_t> remoteRemAddrCb);

  /**
   *
   */
  void GetAllAdvertisedDestinations(std::vector<InetSocketAddress>& );

public: // public variables
  typedef enum {
    Established = 0, /* contains ESTABLISHED/CLOSE_WAIT */
    // TODO rename to restart
    Others = 1,     /**< Composed of SYN_RCVD, SYN_SENT*/
    Closing,        /**< CLOSE_WAIT, FIN_WAIT */
    Maximum         /**< keep it last, used to decalre array */
  } mptcp_container_t;
  // TODO move back to protected/private later on

  /**
   * Local token generated from this connection key
   *
   * \return
   */
//  virtual uint32_t GetToken() const;


  // TODO can be removed
  virtual void
  CompleteFork(Ptr<const Packet> p, const TcpHeader& h, const Address& fromAddress, const Address& toAddress);

protected: // protected methods

  friend class Tcp;
  friend class MpTcpSubflow;


  /*
   *
   * \return addrId the assigned addrId
   * \return bool 
   */
  virtual bool
  AddLocalId (uint8_t *addrId, const Address& address);

  /*
   *
   */ 
  virtual bool
  AddRemoteId (uint8_t addrId, const Address& address);


  /**
   * Expects InetXSocketAddress
   * if no callback set then returns true
   */
  virtual bool NotifyJoinRequest (const Address &from, const Address & toAddress);

  /**
   * Expects Ipv4 (6 not supported yet)
   */
  bool OwnIP(const Address& address) const;
  /**
    * Should be called after having sent a dataFIN
    * Should send a RST on all subflows in state Other
    * and a FIN for Established subflows
    */
  virtual void CloseAllSubflows();

  // MPTCP connection and subflow set up

  virtual int SetupCallback(void);  // Setup SetRxCallback & SetRxCallback call back for a host
  // Same as parent's
//  virtual int  SetupEndpoint (void); // Configure local address for given remote address in a host - it query a routing protocol to find a source

  /*
  Remove subflow from containers
  TODO should also erase its id from the path id manager
  \sf
  \param reset True if closing due to reset
  */
  void
  OnSubflowClosed(Ptr<MpTcpSubflow> sf, bool reset);

  void
  OnSubflowDupAck(Ptr<MpTcpSubflow> sf);

  /**
  \param dataSeq Used to reconstruct the mapping
  Currently used as callback for subflows
  */
  virtual void
  OnSubflowRecv(
                Ptr<MpTcpSubflow> sf
//                SequenceNumber32 dataSeq, Ptr<Socket> sock
                );



  /* close all subflows
  */
  virtual void
  TimeWait();

  /**
   * \brief Creates a DSS option if does not exist and configures it to have a dataack
   * TODO what happens if existing datack already set ?
   */
//  virtual void
//  AppendDataAck(TcpHeader& hdr) const;
//  virtual void AppendDataFin(TcpHeader& header) const;

  /**
  Called when a subflow that initiated the connection
  gets established

  TODO rename into ConnectionSucceeded
  Notify ?
  **/
  virtual void OnSubflowEstablishment(Ptr<MpTcpSubflow>);

  /**
  Called when a subflow that received a connection
  request gets established

  TODO I don't like the name,rename later
  */
  virtual void OnSubflowEstablished(Ptr<MpTcpSubflow> subflow);

  /**
  Should be called when subflows enters FIN_WAIT or LAST_ACK
  */
  virtual void OnSubflowClosing(Ptr<MpTcpSubflow>);


  /**
  Fails if
  **/
//  bool AddLocalAddress(uint8_t&, Port);
// Should generate an Id

//  void SetAddrEventCallback(Callback<bool, Ptr<Socket>, Address, uint8_t> remoteAddAddrCb,
//                          Callback<void, uint8_t> remoteRemAddrCb);
  //virtual RemoteAddAddr
  void NotifyRemoteAddAddr(Address address);
  void NotifyRemoteRemAddr(uint8_t addrId);

  /**
  \bug convert to uint64_t ?
  \note Setting a remote key has the sideeffect of enabling MPTCP on the socket
  */
  void SetPeerKey(uint64_t );

  virtual void
  ConnectionSucceeded(void); // Schedule-friendly wrapper for Socket::NotifyConnectionSucceeded()


  //! disabled
  virtual int DoConnect(void);

  // Transfer operations
//  void ForwardUp(Ptr<Packet> p, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> interface);

  /** Inherit from Socket class: Return data to upper-layer application. Parameter flags
   is not used. Data is returned as a packet of size no larger than maxSize */
  virtual Ptr<Packet> Recv(uint32_t maxSize, uint32_t flags);
  virtual int Send(Ptr<Packet> p, uint32_t flags);

  /**
   * Sending data via subflows with available window size. It sends data only to ESTABLISHED subflows.
   * It sends data by calling SendDataPacket() function.
   * This one is really different from parent class

   *
   * Called by functions: ReceveidAck, NewAck
   * send as  much as possible
   * \return true if it send mappings
   */
  virtual bool SendPendingData(bool withAck = false);


  /** disabled
  */
  virtual uint32_t
  SendDataPacket(SequenceNumber32 seq, uint32_t maxSize, bool withAck);

  //! Disabled
  virtual void ProcessListen  (Ptr<Packet>, const TcpHeader&, const Address&, const Address&);

  // Manage data Tx/Rx
//  virtual Ptr<TcpSocketBase> Fork(void);


  virtual void ReTxTimeout();


  virtual void Retransmit();
  // TODO see if we can remove/override parents

  //! Disabled
  virtual void ReceivedAck ( Ptr<Packet>, const TcpHeader&); // Received an ACK packet

  // MPTCP specfic version
  virtual void ReceivedAck (
    SequenceNumber32 dack
  , Ptr<MpTcpSubflow> sf
  , bool count_dupacks
  );

  //! Disabled
  virtual void ReceivedData ( Ptr<Packet>, const TcpHeader&); // Recv of a data, put into buffer, call L7 to get it if necessary

//  virtual void ProcessDSS( const TcpHeader& tcpHeader, Ptr<TcpOptionMpTcpDSS> dss, Ptr<MpTcpSubflow> sf);
//  virtual void ProcessDSSClosing( Ptr<TcpOptionMpTcpDSS> dss, Ptr<MpTcpSubflow> sf);
//  virtual void ProcessDSSWait( Ptr<TcpOptionMpTcpDSS> dss, Ptr<MpTcpSubflow> sf);
//  virtual void ProcessDSSEstablished( const TcpHeader& tcpHeader, Ptr<TcpOptionMpTcpDSS> dss, Ptr<MpTcpSubflow> sf);


  /** Does nothing */
//  virtual void EstimateRtt (const TcpHeader&);


//  Time ComputeReTxTimeoutForSubflow( Ptr<MpTcpSubflow> );

  /**
   * Part of the logic was implemented but this is non-working.
   * \return Always false
   */
  virtual bool IsInfiniteMappingEnabled() const;
  virtual bool DoChecksum() const;

  //////////////////////////////////////////////////////////////////
  ////  Here follows a list of MPTCP specific *callbacks* triggered by subflows
  ////  on certain events

  /**
   * @param
   * @param mapping
   add count param ?
  */
  virtual void
  OnSubflowDupack(Ptr<MpTcpSubflow> sf, MpTcpMapping mapping);

  virtual void
  OnSubflowRetransmit(Ptr<MpTcpSubflow> sf) ;

//  void LastAckTimeout(uint8_t sFlowIdx);

  virtual void
  OnSubflowNewAck(
//    SequenceNumber32 const& ack,
      Ptr<MpTcpSubflow> sf);


  /**
   * Free space as much as possible
   * Goes through all subflows
   */
  virtual void SyncTxBuffers();

  /**
   * Free space as much as possible
   * TODO rename or move to MpTcpSubflow
   */
  virtual void SyncTxBuffers(Ptr<MpTcpSubflow> sf);

  /**
   *  inherited from parent: update buffers
   * @brief Called from subflows when they receive DATA-ACK. For now calls parent fct
   */
  virtual void NewAck(SequenceNumber32 const& dataLevelSeq);

  //! disabled
  virtual void SendEmptyPacket(TcpHeader& header);

  // Not implemented yet
//  virtual void
//  NewAck(SequenceNumber64 const& dataLevelSeq);


  /**
   *
   */
//  virtual void ProcessMpTcpOptions(TcpHeader h, Ptr<MpTcpSubflow> sf);


  virtual void OnTimeWaitTimeOut();


protected: // protected variables

  virtual void UpdateTxBuffer();

  friend class TcpL4Protocol;
  // TODO make it so this is not necessary
  // like putting m_nextTxSequence in MpTcpScheduler and make it friend ?
  friend class MpTcpSchedulerRoundRobin;
  friend class MpTcpSchedulerFastestRTT;
  friend class MpTcpSchedulerOwd;


  /**
  called by TcpL4protocol when receiving an MP_JOIN taht does not fit
  to any Ipv4endpoint. Thus twe create one.
  **/
  virtual Ipv4EndPoint*
  NewSubflowRequest(
    Ptr<const Packet> p,
    const TcpHeader & header,
    const Address & fromAddress,
    const Address & toAddress,
    Ptr<const TcpOptionMpTcpJoin> join
  );

  // ?
//  int CloseSubflow(Ptr<MpTcpSubflow> sf);


  /**
   * TODO should accept a stream
   * Dump 
   */
  virtual void DumpSubflows (std::ostream &os) const;

  /**
   *
   */
  SubflowList m_subflows[Maximum];

  Callback<bool, Ptr<Socket>, Address, uint8_t > m_onRemoteAddAddr;  //!< return true to create a subflow
//  Callback<bool, Ptr<Socket>, Address, uint8_t > m_onNewLocalIp;  //!< return true to create a subflow
  Callback<void, uint8_t > m_onAddrDeletion;    // return true to create a subflow

//  Callback<void, const MpTcpAddressInfo& > m_onRemAddr;

//  virtual void OnAddAddress(MpTcpAddressInfo);
//  virtual void OnRemAddress();

public:

  /**
  ONLY TEMPORARY
  Used to export a whole range of statistics to csv files (filenames hardcoded).
  This would likely need a rework before upstream, for instance to allow
  enabling/disabling
  **/
//  virtual void
//  SetupTracing(std::string prefix);
  // Returns result based on m_tracePrefix
  bool
  IsTracingEnabled() const;
//  virtual void
//  SetupSubflowTracing(Ptr<MpTcpSubflow> sf);
  std::string m_tracePrefix;      //!< help naming csv files, TODO should be removed
//  int m_prefixCounter;      //!< TODO remove and put in a helper
 /****** END TRACING *****/

protected:
  virtual void CreateScheduler (TypeId schedulerTypeId);

  // TODO rename to peerIdManager ?
  Ptr<MpTcpPathIdManager> m_remoteIdManager;  //!< Keep track of advertised ADDR id advertised by remote endhost
  Ptr<MpTcpPathIdManager> m_localIdManager;  //!< Keep track of advertised ADDR id advertised by remote endhost


  /***
  TODO the scheduler is so closely
  Rename into MpTcpScheduler
  ***/
  Ptr<MpTcpScheduler> m_scheduler;  //!<

  // TODO make private ? check what it does
  // should be able to rmeove one
  bool m_server;  //!< True if this socket is the result of a fork, ie it was originally LISTENing

private:
  // TODO rename into m_localKey  and move tokens into subflow (maybe not even needed)
//  uint64_t m_localKey;    //!< Store local host token, generated during the 3-way handshake
//  uint32_t m_localToken;  //!< Generated from key

  uint64_t m_peerKey; //!< Store remote host token
  uint32_t m_peerToken;

  bool     m_doChecksum;  //!< Compute the checksum. Negociated during 3WHS. Unused

  bool     m_receivedDSS;  //!< True if we received at least one DSS

  bool     m_generatedIdsn; //!<
  

private:


  /* Utility function used when a subflow changes state
   *  Research of the subflow is done
   * \warn This function does not check if the destination container is
   */
  void MoveSubflow(Ptr<MpTcpSubflow> sf, mptcp_container_t to);

  /**
   * Asserts if from == to
   */
  void MoveSubflow(Ptr<MpTcpSubflow> sf, mptcp_container_t from, mptcp_container_t to);

  Callback<void, Ptr<MpTcpSubflow> > m_subflowConnectionSucceeded;  //!< connection succeeded callback
  Callback<void, Ptr<MpTcpSubflow> > m_subflowConnectionFailure;     //!< connection failed callback
//  Callback<void, Ptr<Socket> >                   m_normalClose;          //!< connection closed callback
//  Callback<void, Ptr<Socket> >                   m_errorClose;           //!< connection closed due to errors callback
  Callback<bool, Ptr<MpTcpSocketBase>, const Address &, const Address & >       m_joinRequest;    //!< connection request callback
  Callback<void, Ptr<MpTcpSubflow> >    m_subflowCreated; //!< connection created callback

// , const Address &, bool master
//  Callback<void, Ptr<MpTcpSubflow> >    m_subflowConnectionSucceeded; //!< connection created callback


    //!
    TypeId m_subflowTypeId;
    TypeId m_schedulerTypeId;
};

}   //namespace ns3

#endif /* MP_TCP_SOCKET_BASE_H */
