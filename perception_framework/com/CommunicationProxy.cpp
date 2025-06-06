#include "CommunicationProxy.hpp"
#include "FifoComm.hpp"
#include "Logger.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Default thread pool size
constexpr size_t DEFAULT_THREAD_POOL_SIZE = 3;

// CommunicationProxy implementation
CommunicationProxy& CommunicationProxy::getInstance() {
    static CommunicationProxy instance;
    return instance;
}

CommunicationProxy::CommunicationProxy() {
    // Constructor
}

CommunicationProxy::~CommunicationProxy() {
    stop();
}

bool CommunicationProxy::initialize(const std::string& basePath, CommRole role) {
    if(isInitialized_) {
        LOG_WARN("Communication proxy already initialized");
        return true;
    }

    LOG_INFO("Initializing communication proxy...");
    
    // Set initial connection state to connecting
    setConnectionState(ConnectionState::CONNECTING);
    
    // Create FIFO communication implementation
    commImpl_ = std::make_unique<FifoCommImpl>(basePath, role);
    
    // Initialize communication implementation
    if (!commImpl_->initialize()) {
        LOG_ERROR("Failed to initialize CommunicationProxy");
        setConnectionState(ConnectionState::DISCONNECTED);
        return false;
    }
    
    // Create thread pool
    threadPool_ = std::make_unique<utils::ThreadPool>(DEFAULT_THREAD_POOL_SIZE);
    
    isInitialized_ = true;
    LOG_INFO("Communication proxy initialized successfully");
    return true;
}

bool CommunicationProxy::initialize() {
    return initialize("/tmp/orbbec_camera", CommRole::AUTO);
}

void CommunicationProxy::start() {
    if(!isInitialized_) {
        LOG_ERROR("Cannot start communication proxy: not initialized");
        return;
    }

    if(isRunning_) {
        LOG_WARN("Communication proxy already running");
        return;
    }

    isRunning_ = true;
    LOG_INFO("Starting communication proxy...");

    // Start message receiving thread
    receivingThread_ = std::thread(&CommunicationProxy::messageReceivingThread, this);
    
    // Start different initialization flow based on role
    if (commImpl_->isServer()) {
        LOG_INFO("Server started, waiting for client connection...");
    } else {
        // Client needs to try connecting to server
        LOG_INFO("Client started, attempting to connect to server...");
    }
    
    LOG_INFO("Communication proxy started");
}

void CommunicationProxy::stop() {
    if(!isRunning_) {
        return;
    }

    LOG_INFO("Stopping communication proxy...");
    isRunning_ = false;
    
    // Notify all waiting threads
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connectionCondition_.notify_all();
    }
    
    // Wait for thread to end
    if(receivingThread_.joinable()) {
        receivingThread_.join();
    }
    
    // Destroy thread pool (destructor will handle thread shutdown)
    threadPool_.reset();
    
    // Clean up communication resources
    if (commImpl_) {
        commImpl_->cleanup();
    }
    
    // Update connection state
    setConnectionState(ConnectionState::DISCONNECTED);
    
    LOG_INFO("Communication proxy stopped");
}

bool CommunicationProxy::sendMessage(MessageType type, const std::string& content) {
    if(!isRunning_) {
        LOG_ERROR("Cannot send message: communication proxy not running");
        return false;
    }
    
    // Check connection state
    if (connectionState_ != ConnectionState::CONNECTED) {
        // Try to establish connection
        if (type != MessageType::HEARTBEAT) {
            LOG_WARN("Failed to send message: not connected, message type=", static_cast<int>(type), ", content=", content);
        }
        return false;
    }
    
    // Create message, set priority
    MessagePriority priority = getMessagePriority(type);
    Message message(type, content, priority);
    
    LOG_DEBUG("Sending message: type=", static_cast<int>(type), ", content=", content);
    
    // Serialize message and send through communication implementation
    bool result = commImpl_->sendMessage(message.serialize());
    
    // If send fails, connection may be broken
    if (!result) {
        setConnectionState(ConnectionState::DISCONNECTED);
    }
    
    return result;
}

void CommunicationProxy::registerCallback(MessageType type, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_[type] = callback;
    LOG_DEBUG("Registered callback for message type: ", static_cast<int>(type));
}

void CommunicationProxy::unregisterCallback(MessageType type) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.erase(type);
    LOG_DEBUG("Unregistered callback for message type: ", static_cast<int>(type));
}

CommunicationProxy::MessagePriority CommunicationProxy::getMessagePriority(MessageType type) {
    // Set priority for different message types
    switch (type) {
        case MessageType::HEARTBEAT:
            return MessagePriority::HIGH;  // Heartbeat messages have highest priority
            
        case MessageType::ERROR:
        case MessageType::STATUS_REPORT:
            return MessagePriority::HIGH;  // Error and status reports also have high priority
            
        case MessageType::COMMAND:
            return MessagePriority::NORMAL;  // Command messages have normal priority
            
        case MessageType::METADATA:
        case MessageType::DATA:
            return MessagePriority::LOW;  // Data messages have low priority
            
        default:
            return MessagePriority::NORMAL;
    }
}

void CommunicationProxy::messageReceivingThread() {
    LOG_DEBUG("Message receiving thread started");
    
    // Flag for first successful message reception
    bool firstMessageReceived = false;
    
    // Message processing counter
    int messagesProcessedInBatch = 0;
    const int MAX_MESSAGES_PER_BATCH = 10; // Maximum messages to process in a single loop
    
    while(isRunning_) {
        try {
            // Reset batch processing counter
            messagesProcessedInBatch = 0;
            
            // Use polling to receive messages, process all available messages in loop
            while(isRunning_ && messagesProcessedInBatch < MAX_MESSAGES_PER_BATCH) {
                std::string messageData;
                if(commImpl_->receiveMessage(messageData)) {
                    // If this is the first message received, update connection state
                    if (!firstMessageReceived) {
                        firstMessageReceived = true;
                        setConnectionState(ConnectionState::CONNECTED);
                        LOG_INFO("Successfully received first message, connection established");
                    }
                    
                    // Deserialize message
                    Message message = Message::deserialize(messageData);
                    
                    LOG_DEBUG("Received message: type=", static_cast<int>(message.type), 
                             ", content=", message.content);
                    
                    // Process high priority messages synchronously, submit others to thread pool
                    if (message.priority == MessagePriority::HIGH) {
                        // Process high priority messages synchronously (like heartbeat)
                        processReceivedMessage(message);
                    } else {
                        // Process other messages asynchronously
                        Message msgCopy = message; // Create a copy of the message
                        threadPool_->submit([this, msgCopy]() {
                            processReceivedMessage(msgCopy);
                        });
                    }
                    
                    // Increment batch processing counter
                    messagesProcessedInBatch++;
                } else {
                    // No more messages to process, exit inner loop
                    break;
                }
            }
            
            // If a large number of messages were processed in a single loop, log it
            if (messagesProcessedInBatch >= MAX_MESSAGES_PER_BATCH) {
                LOG_WARN("Processed large number of messages in a single loop: ", messagesProcessedInBatch, 
                        ", possible message accumulation");
            }
            
            // Check connection state
            if (commImpl_->isConnected()) {
                if (connectionState_ != ConnectionState::CONNECTED) {
                    setConnectionState(ConnectionState::CONNECTED);
                }
            } else {
                if (connectionState_ != ConnectionState::DISCONNECTED) {
                    setConnectionState(ConnectionState::DISCONNECTED);
                }
            }
            
            // Sleep to avoid busy loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        catch(const std::exception& e) {
            LOG_ERROR("Message receiving thread exception: ", e.what());
            setConnectionState(ConnectionState::DISCONNECTED);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LOG_DEBUG("Message receiving thread stopped");
}

void CommunicationProxy::processReceivedMessage(const Message& message) {
    try {
        // Call registered callback for message type
        std::lock_guard<std::mutex> lock(callbackMutex_);
        auto it = callbacks_.find(message.type);
        if (it != callbacks_.end() && it->second) {
            it->second(message);
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("Message callback exception: ", e.what());
    }
}

void CommunicationProxy::registerConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connectionCallback_ = callback;
}

void CommunicationProxy::unregisterConnectionCallback() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connectionCallback_ = nullptr;
}

bool CommunicationProxy::waitForConnection(int timeoutMs) {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    
    if (connectionState_ == ConnectionState::CONNECTED) {
        return true;
    }
    
    if (timeoutMs <= 0) {
        // Wait indefinitely
        connectionCondition_.wait(lock, [this]() {
            return connectionState_ == ConnectionState::CONNECTED || !isRunning_;
        });
    } else {
        // Wait with timeout
        return connectionCondition_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
            return connectionState_ == ConnectionState::CONNECTED || !isRunning_;
        });
    }
    
    return connectionState_ == ConnectionState::CONNECTED;
}

void CommunicationProxy::setConnectionState(ConnectionState newState) {
    ConnectionState oldState = connectionState_;
    
    if (oldState != newState) {
        connectionState_ = newState;
        
        LOG_INFO("Communication connection state changed: ", static_cast<int>(newState));
        
        // Notify waiters
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connectionCondition_.notify_all();
        }
        
        // Call callback if registered
        if (connectionCallback_) {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            if (connectionCallback_) {
                connectionCallback_(newState);
            }
        }
    }
} 