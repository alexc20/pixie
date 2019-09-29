// Code generated by MockGen. DO NOT EDIT.
// Source: server.go

// Package mock_controller is a generated GoMock package.
package mock_controller

import (
	gomock "github.com/golang/mock/gomock"
	reflect "reflect"
)

// MockK8sAPI is a mock of K8sAPI interface
type MockK8sAPI struct {
	ctrl     *gomock.Controller
	recorder *MockK8sAPIMockRecorder
}

// MockK8sAPIMockRecorder is the mock recorder for MockK8sAPI
type MockK8sAPIMockRecorder struct {
	mock *MockK8sAPI
}

// NewMockK8sAPI creates a new mock instance
func NewMockK8sAPI(ctrl *gomock.Controller) *MockK8sAPI {
	mock := &MockK8sAPI{ctrl: ctrl}
	mock.recorder = &MockK8sAPIMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use
func (m *MockK8sAPI) EXPECT() *MockK8sAPIMockRecorder {
	return m.recorder
}

// CreateTLSSecret mocks base method
func (m *MockK8sAPI) CreateTLSSecret(name, key, cert string) error {
	ret := m.ctrl.Call(m, "CreateTLSSecret", name, key, cert)
	ret0, _ := ret[0].(error)
	return ret0
}

// CreateTLSSecret indicates an expected call of CreateTLSSecret
func (mr *MockK8sAPIMockRecorder) CreateTLSSecret(name, key, cert interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CreateTLSSecret", reflect.TypeOf((*MockK8sAPI)(nil).CreateTLSSecret), name, key, cert)
}

// GetPodNamesForService mocks base method
func (m *MockK8sAPI) GetPodNamesForService(name string) ([]string, error) {
	ret := m.ctrl.Call(m, "GetPodNamesForService", name)
	ret0, _ := ret[0].([]string)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetPodNamesForService indicates an expected call of GetPodNamesForService
func (mr *MockK8sAPIMockRecorder) GetPodNamesForService(name interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetPodNamesForService", reflect.TypeOf((*MockK8sAPI)(nil).GetPodNamesForService), name)
}

// DeletePod mocks base method
func (m *MockK8sAPI) DeletePod(name string) error {
	ret := m.ctrl.Call(m, "DeletePod", name)
	ret0, _ := ret[0].(error)
	return ret0
}

// DeletePod indicates an expected call of DeletePod
func (mr *MockK8sAPIMockRecorder) DeletePod(name interface{}) *gomock.Call {
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "DeletePod", reflect.TypeOf((*MockK8sAPI)(nil).DeletePod), name)
}
