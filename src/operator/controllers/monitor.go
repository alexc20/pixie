/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/apis/meta/internalversion"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	watchClient "k8s.io/client-go/tools/watch"
	"sigs.k8s.io/controller-runtime/pkg/client"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"

	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/status"
)

const (
	// The name label of the cloud-conn pod.
	cloudConnName = "vizier-cloud-connector"

	// How often we should ping the vizier pods for status updates.
	statuszCheckInterval = 20 * time.Second
)

// HTTPClient is the interface for a simple HTTPClient which can execute "Get".
type HTTPClient interface {
	Get(string) (resp *http.Response, err error)
}

// VizierMonitor is responsible for watching the k8s API and statusz endpoints to compile a reason and state
// for the overall Vizier instance.
type VizierMonitor struct {
	clientset  *kubernetes.Clientset
	httpClient HTTPClient
	ctx        context.Context
	cancel     func()

	namespace      string
	namespacedName types.NamespacedName

	states map[string]*v1.Pod
	lastRV string

	vzUpdate func(context.Context, client.Object, ...client.UpdateOption) error
	vzGet    func(context.Context, types.NamespacedName, client.Object) error
}

// InitAndStartMonitor initializes and starts the status monitor for the Vizier.
func (m *VizierMonitor) InitAndStartMonitor() error {
	// Initialize current state.
	tr := &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}
	m.httpClient = &http.Client{Transport: tr}
	m.ctx, m.cancel = context.WithCancel(context.Background())
	m.states = make(map[string]*v1.Pod)

	err := m.initState()
	if err != nil {
		return err
	}

	// Watch for future updates in the namespace.
	go m.watchK8sAPI()

	// Start goroutine for periodically pinging statusz endpoints and
	// reconciling the Vizier status.
	go m.runReconciler()

	return nil
}

func (m *VizierMonitor) initState() error {
	watcher := cache.NewListWatchFromClient(m.clientset.CoreV1().RESTClient(), "pods", m.namespace, fields.Everything())
	pods, err := watcher.List(metav1.ListOptions{})
	if err != nil {
		return err
	}

	// Convert to pod list.
	podList, lastRV, err := getPodList(pods)
	if err != nil {
		return err
	}
	m.lastRV = lastRV

	// Populate vizierStates with current pod state.
	for _, pod := range *podList {
		m.handlePod(pod)
	}

	return nil
}

func getPodList(o runtime.Object) (*[]v1.Pod, string, error) {
	podList, ok := o.(*v1.PodList)

	if ok {
		return &podList.Items, podList.ResourceVersion, nil
	}

	internalList, ok := o.(*internalversion.List)
	if !ok {
		return nil, "", errors.New("Could not get pod list")
	}

	typedList := v1.PodList{}
	for _, i := range internalList.Items {
		item, ok := i.(*v1.Pod)
		if !ok {
			return nil, "", errors.New("Could not get pod list")
		}
		typedList.Items = append(typedList.Items, *item)
	}

	return &typedList.Items, internalList.ResourceVersion, nil
}

func (m *VizierMonitor) handlePod(pod v1.Pod) {
	// We label all of our vizier pods with a name=<componentName>.
	// For now, this assumes no replicas. If a new pod starts up, it will replace
	// the status of the previous pod.
	// In the future we may add special handling for PEMs/multiple kelvins.
	if name, ok := pod.ObjectMeta.Labels["name"]; ok {
		if st, stOk := m.states[name]; stOk {
			if st.ObjectMeta.Name != pod.ObjectMeta.Name && pod.ObjectMeta.CreationTimestamp.Before(&st.ObjectMeta.CreationTimestamp) {
				return
			}
		}
		m.states[name] = &pod
	}
}

func (m *VizierMonitor) watchK8sAPI() {
	for {
		watcher := cache.NewListWatchFromClient(m.clientset.CoreV1().RESTClient(), "pods", m.namespace, fields.Everything())
		retryWatcher, err := watchClient.NewRetryWatcher(m.lastRV, watcher)
		if err != nil {
			log.WithError(err).Fatal("Could not start watcher for pods")
		}

		resCh := retryWatcher.ResultChan()
		runWatcher := true
		for runWatcher {
			select {
			case <-m.ctx.Done():
				log.Info("Received cancel, stopping K8s watcher")
				return
			case c := <-resCh:
				s, ok := c.Object.(*metav1.Status)
				if ok && s.Status == metav1.StatusFailure {
					continue
				}

				// Update the lastRV, so that if the watcher restarts, it starts at the correct resource version.
				o, ok := c.Object.(*v1.Pod)
				if !ok {
					continue
				}

				m.lastRV = o.ObjectMeta.ResourceVersion

				m.handlePod(*o)
			}
		}
	}
}

// runReconciler is responsible for periodically pinging the Vizier pods to determine their self-reported state.
// The reconciler combines this information with the K8s API information to determine an overall Vizier state/reason/message.
func (m *VizierMonitor) runReconciler() {
	t := time.NewTicker(statuszCheckInterval)
	for {
		select {
		case <-m.ctx.Done():
			log.Info("Received cancel, stopping status reconciler")
			return
		case <-t.C:
			state, reason := ReconcileStatus(m.httpClient, m.states)

			vz := &pixiev1alpha1.Vizier{}
			err := m.vzGet(context.Background(), m.namespacedName, vz)
			if err != nil {
				log.WithError(err).Error("Failed to get vizier")
				continue
			}

			vz.Status.VizierPhase = state
			vz.Status.VizierReason = reason
			vz.Status.Message = status.GetMessageFromReason(reason)
			err = m.vzUpdate(context.Background(), vz)
			if err != nil {
				log.WithError(err).Error("Failed to update vizier status")
			}
		}
	}
}

// ReconcileStatus takes a set of Vizier pods and determines the overall status based on the pod states and
// their statusz endpoints.
func ReconcileStatus(client HTTPClient, pods map[string]*v1.Pod) (pixiev1alpha1.VizierPhase, string) {
	// Check cloudConn first, to ensure that the vizier has successfully connected to a Pixie cloud.
	if ccPod, ok := pods[cloudConnName]; ok {
		if ccPod.Status.Phase == v1.PodPending {
			return pixiev1alpha1.VizierPhaseUpdating, ""
		}

		if ccPod.Status.Phase != v1.PodRunning {
			return pixiev1alpha1.VizierPhaseUnhealthy, ""
		}
		log.Info(pods)
		// Ping cloudConn's statusz.
		ok, status := GetPodStatus(client, ccPod)
		if !ok {
			return pixiev1alpha1.VizierPhaseUnhealthy, status
		}
	} else {
		return pixiev1alpha1.VizierPhaseDisconnected, ""
	}

	// TODO(michellenguyen): If cloudConn is because it can't run a basic query,
	// check why the other pod statuses may be failing.
	return pixiev1alpha1.VizierPhaseHealthy, ""
}

// GetPodStatus gets a pod's status by pinging its statusz endpoint.
func GetPodStatus(client HTTPClient, pod *v1.Pod) (bool, string) {
	podIP := pod.Status.PodIP
	// Assume that the statusz endpoint is on the first port in the first container.
	var port int32
	if len(pod.Spec.Containers) > 0 && len(pod.Spec.Containers[0].Ports) > 0 {
		port = pod.Spec.Containers[0].Ports[0].ContainerPort
	}

	resp, err := client.Get(fmt.Sprintf("https://%s:%d/statusz", podIP, port))
	if err != nil {
		log.WithError(err).Info("Error making statusz call")
		return false, ""
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusOK {
		return true, ""
	}

	body, err := io.ReadAll(resp.Body)

	if err != nil {
		return false, ""
	}

	return false, strings.TrimSpace(string(body))
}

// Quit stops the VizierMonitor from monitoring the vizier in the given namespace.
func (m *VizierMonitor) Quit() {
	if m.ctx != nil {
		m.cancel()
	}
}
